#!/usr/bin/env python3
"""M2 benchmark runner for StoryToVideo Renew.

Runs end-to-end workflow benchmarks against the local FastAPI server and
emits structured metrics to bench.json plus a markdown report.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import shutil
import signal
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def percentile(values: List[float], p: float) -> Optional[float]:
    if not values:
        return None
    ordered = sorted(values)
    if len(ordered) == 1:
        return float(ordered[0])
    rank = (len(ordered) - 1) * p
    lower = math.floor(rank)
    upper = math.ceil(rank)
    if lower == upper:
        return float(ordered[lower])
    weight = rank - lower
    return float(ordered[lower] * (1.0 - weight) + ordered[upper] * weight)


def run_cmd(cmd: List[str]) -> Tuple[int, str]:
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
        out = (proc.stdout or "") + (proc.stderr or "")
        return proc.returncode, out.strip()
    except Exception as exc:  # noqa: BLE001
        return 1, str(exc)


def read_rss_mb(pid: int) -> Optional[float]:
    status_file = Path(f"/proc/{pid}/status")
    if not status_file.exists():
        return None
    try:
        for line in status_file.read_text().splitlines():
            if line.startswith("VmRSS:"):
                parts = line.split()
                if len(parts) >= 2:
                    return float(parts[1]) / 1024.0
    except Exception:  # noqa: BLE001
        return None
    return None


def read_total_rss_mb(server_pid: int) -> Optional[float]:
    server_rss = read_rss_mb(server_pid)
    self_rss = read_rss_mb(os.getpid())
    if server_rss is None and self_rss is None:
        return None
    return (server_rss or 0.0) + (self_rss or 0.0)


def read_peak_vram_gib() -> Optional[float]:
    if shutil.which("nvidia-smi") is None:
        return None
    code, output = run_cmd(
        ["nvidia-smi", "--query-gpu=memory.used", "--format=csv,noheader,nounits"]
    )
    if code != 0 or not output:
        return None
    line = output.splitlines()[0].strip()
    try:
        used_mib = float(line)
    except ValueError:
        return None
    return used_mib / 1024.0


def ffmpeg_has_nvenc() -> bool:
    if shutil.which("ffmpeg") is None:
        return False
    code, output = run_cmd(["ffmpeg", "-hide_banner", "-encoders"])
    return code == 0 and "h264_nvenc" in output


def http_json(
    method: str,
    url: str,
    payload: Optional[Dict[str, Any]] = None,
    timeout_s: float = 120.0,
) -> Tuple[int, Any, float]:
    data = None
    headers = {}
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"

    request = urllib.request.Request(url=url, method=method, data=data, headers=headers)
    start = time.perf_counter()
    try:
        with urllib.request.urlopen(request, timeout=timeout_s) as response:
            body_bytes = response.read()
            status = int(response.status)
    except urllib.error.HTTPError as exc:
        body_bytes = exc.read()
        status = int(exc.code)
    except urllib.error.URLError as exc:
        elapsed_ms = (time.perf_counter() - start) * 1000.0
        return 0, {"error": str(exc)}, elapsed_ms
    elapsed_ms = (time.perf_counter() - start) * 1000.0

    body_text = body_bytes.decode("utf-8", errors="replace") if body_bytes else ""
    try:
        body_json = json.loads(body_text) if body_text else {}
    except json.JSONDecodeError:
        body_json = {"raw": body_text}
    return status, body_json, elapsed_ms


def should_retry_status(status: int) -> bool:
    return status == 0 or status == 429 or status >= 500


def call_with_retry(
    method: str,
    url: str,
    payload: Optional[Dict[str, Any]],
    timeout_s: float,
    max_retries: int,
    initial_backoff_ms: int,
    max_backoff_ms: int,
) -> Tuple[int, Any, float, int]:
    retries = 0
    backoff_ms = max(0, initial_backoff_ms)
    while True:
        status, response, elapsed_ms = http_json(method, url, payload=payload, timeout_s=timeout_s)
        if status == 200:
            return status, response, elapsed_ms, retries
        if retries >= max_retries or not should_retry_status(status):
            return status, response, elapsed_ms, retries
        time.sleep(backoff_ms / 1000.0)
        retries += 1
        backoff_ms = min(max_backoff_ms, max(backoff_ms * 2, 1))


@dataclass
class ScenarioResult:
    name: str
    runs: int
    success_count: int
    success_rate: float
    p50_s: Optional[float]
    p95_s: Optional[float]
    cancel_p50_ms: Optional[float]
    cancel_p95_ms: Optional[float]
    peak_total_rss_mb: Optional[float]
    peak_vram_gib: Optional[float]
    encoder_usage: Dict[str, int]
    failures: List[Dict[str, Any]]
    run_details: List[Dict[str, Any]]


class ServerHandle:
    def __init__(self, python_bin: Path, server_dir: Path, env: Dict[str, str]) -> None:
        self._python_bin = python_bin
        self._server_dir = server_dir
        self._env = env
        self.proc: Optional[subprocess.Popen[str]] = None
        self._log_lines: List[str] = []
        self._log_lock = threading.Lock()
        self._log_thread: Optional[threading.Thread] = None

    def start(self) -> None:
        self.proc = subprocess.Popen(
            [str(self._python_bin), "-m", "app.main"],
            cwd=self._server_dir,
            env=self._env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        self._log_thread = threading.Thread(target=self._drain_logs, daemon=True)
        self._log_thread.start()

    def _drain_logs(self) -> None:
        if not self.proc or not self.proc.stdout:
            return
        for line in self.proc.stdout:
            with self._log_lock:
                self._log_lines.append(line.rstrip("\n"))
                # Keep only recent lines to avoid unbounded memory.
                if len(self._log_lines) > 400:
                    self._log_lines = self._log_lines[-400:]

    def wait_healthy(self, base_url: str, timeout_s: float = 20.0) -> None:
        deadline = time.time() + timeout_s
        while time.time() < deadline:
            if self.proc and self.proc.poll() is not None:
                raise RuntimeError(
                    f"Server exited early with code {self.proc.returncode}.\n{self.logs_tail()}"
                )
            try:
                status, payload, _ = http_json("GET", f"{base_url}/healthz", timeout_s=1.5)
                if status == 200 and isinstance(payload, dict) and payload.get("status") == "healthy":
                    return
            except Exception:  # noqa: BLE001
                pass
            time.sleep(0.25)
        raise RuntimeError(f"Server did not become healthy in {timeout_s}s.\n{self.logs_tail()}")

    def logs_tail(self, lines: int = 40) -> str:
        with self._log_lock:
            tail = self._log_lines[-lines:]
        return "\n".join(tail)

    def stop(self) -> None:
        if not self.proc:
            return
        if self.proc.poll() is None:
            self.proc.send_signal(signal.SIGTERM)
            try:
                self.proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(timeout=3)
        if self._log_thread and self._log_thread.is_alive():
            self._log_thread.join(timeout=1.0)


def measure_cancel_latency(base_url: str, run_tag: str) -> Optional[float]:
    request_id = f"req-cancel-{run_tag}"
    trace_id = f"trace-cancel-{run_tag}"
    payload = {
        "trace_id": trace_id,
        "request_id": request_id,
        "prompt": "a castle under moonlight",
        "width": 512,
        "height": 512,
        "num_inference_steps": 20,
    }

    request_done = threading.Event()
    request_error: List[str] = []

    def worker() -> None:
        try:
            http_json("POST", f"{base_url}/v1/imagegen", payload=payload, timeout_s=300)
        except Exception as exc:  # noqa: BLE001
            request_error.append(str(exc))
        finally:
            request_done.set()

    thread = threading.Thread(target=worker, daemon=True)
    thread.start()
    time.sleep(0.05)

    cancel_start = time.perf_counter()
    try:
        http_json("POST", f"{base_url}/v1/cancel/{request_id}", timeout_s=30)
    except Exception:
        pass

    if not request_done.wait(timeout=30):
        return None
    return (time.perf_counter() - cancel_start) * 1000.0


def run_single_workflow(
    base_url: str,
    output_dir: Path,
    run_index: int,
    nvenc_available: bool,
    provider: str,
    http_max_retries: int,
    http_initial_backoff_ms: int,
    http_max_backoff_ms: int,
) -> Dict[str, Any]:
    trace_id = f"bench-{int(time.time() * 1000)}-{run_index}"
    start = time.perf_counter()
    total_retries = 0

    story_payload = {
        "trace_id": trace_id,
        "request_id": f"req-story-{run_index}",
        "story_text": (
            "Once upon a time, a brave knight crossed mountains to rescue a village. "
            "He entered a dragon cave, fought with courage, and returned safely."
        ),
        "target_duration": 30.0,
        "scene_count": 4,
    }
    status, storyboard, _, retries = call_with_retry(
        "POST",
        f"{base_url}/v1/storyboard",
        payload=story_payload,
        timeout_s=120,
        max_retries=http_max_retries,
        initial_backoff_ms=http_initial_backoff_ms,
        max_backoff_ms=http_max_backoff_ms,
    )
    total_retries += retries
    if status != 200:
        return {
            "success": False,
            "stage": "storyboard",
            "status": status,
            "response": storyboard,
            "retry_count": total_retries,
        }

    scenes = storyboard.get("scenes", [])
    if not scenes:
        return {"success": False, "stage": "storyboard", "status": status, "response": storyboard}

    scene_assets = []
    for idx, scene in enumerate(scenes):
        image_payload = {
            "trace_id": trace_id,
            "request_id": f"req-image-{run_index}-{idx}",
            "prompt": scene.get("visual_description") or "cinematic landscape",
            "width": 512,
            "height": 512,
            "num_inference_steps": 20,
        }
        status, image_data, _, retries = call_with_retry(
            "POST",
            f"{base_url}/v1/imagegen",
            payload=image_payload,
            timeout_s=300,
            max_retries=http_max_retries,
            initial_backoff_ms=http_initial_backoff_ms,
            max_backoff_ms=http_max_backoff_ms,
        )
        total_retries += retries
        if status != 200:
            return {
                "success": False,
                "stage": "imagegen",
                "status": status,
                "response": image_data,
                "retry_count": total_retries,
            }

        tts_payload = {
            "trace_id": trace_id,
            "request_id": f"req-tts-{run_index}-{idx}",
            "text": scene.get("narration") or "scene narration",
            "speed": 1.0,
        }
        status, tts_data, _, retries = call_with_retry(
            "POST",
            f"{base_url}/v1/tts",
            payload=tts_payload,
            timeout_s=180,
            max_retries=http_max_retries,
            initial_backoff_ms=http_initial_backoff_ms,
            max_backoff_ms=http_max_backoff_ms,
        )
        total_retries += retries
        if status != 200:
            return {
                "success": False,
                "stage": "tts",
                "status": status,
                "response": tts_data,
                "retry_count": total_retries,
            }

        scene_assets.append(
            {
                "scene_number": idx,
                "image_path": image_data.get("image_path"),
                "audio_path": tts_data.get("audio_path"),
                "duration_seconds": float(scene.get("duration_seconds", 7.5)),
            }
        )

    output_path = output_dir / f"bench_{trace_id}.mp4"
    compose_payload = {
        "trace_id": trace_id,
        "request_id": f"req-compose-{run_index}",
        "scenes": scene_assets,
        "output_path": str(output_path),
        "fps": 24,
        "codec": "h264_nvenc",
    }
    status, compose_data, _, retries = call_with_retry(
        "POST",
        f"{base_url}/v1/compose",
        payload=compose_payload,
        timeout_s=600,
        max_retries=http_max_retries,
        initial_backoff_ms=http_initial_backoff_ms,
        max_backoff_ms=http_max_backoff_ms,
    )
    total_retries += retries
    if status != 200:
        return {
            "success": False,
            "stage": "compose",
            "status": status,
            "response": compose_data,
            "retry_count": total_retries,
        }

    file_exists = output_path.exists() and output_path.stat().st_size > 0
    elapsed_s = time.perf_counter() - start
    encoder_used = "libx264"
    if provider == "local_gpu" and nvenc_available:
        encoder_used = "h264_nvenc"

    return {
        "success": bool(file_exists),
        "e2e_s": elapsed_s,
        "trace_id": trace_id,
        "video_path": str(output_path),
        "encoder": encoder_used,
        "size_bytes": output_path.stat().st_size if output_path.exists() else 0,
        "retry_count": total_retries,
    }


def summarize_scenario(
    name: str,
    run_details: List[Dict[str, Any]],
    peak_rss_mb: Optional[float],
    peak_vram_gib: Optional[float],
) -> ScenarioResult:
    successes = [run for run in run_details if run.get("success")]
    failures = [run for run in run_details if not run.get("success")]
    durations = [float(run["e2e_s"]) for run in successes if "e2e_s" in run]
    cancel_latencies = [float(run["cancel_latency_ms"]) for run in run_details if run.get("cancel_latency_ms")]

    encoder_usage: Dict[str, int] = {"h264_nvenc": 0, "libx264": 0}
    for run in successes:
        encoder = run.get("encoder")
        if encoder in encoder_usage:
            encoder_usage[encoder] += 1

    total_runs = len(run_details)
    success_count = len(successes)
    success_rate = (success_count / total_runs) if total_runs else 0.0

    return ScenarioResult(
        name=name,
        runs=total_runs,
        success_count=success_count,
        success_rate=success_rate,
        p50_s=percentile(durations, 0.5),
        p95_s=percentile(durations, 0.95),
        cancel_p50_ms=percentile(cancel_latencies, 0.5),
        cancel_p95_ms=percentile(cancel_latencies, 0.95),
        peak_total_rss_mb=peak_rss_mb,
        peak_vram_gib=peak_vram_gib,
        encoder_usage=encoder_usage,
        failures=failures,
        run_details=run_details,
    )


def scenario_to_dict(result: ScenarioResult) -> Dict[str, Any]:
    return {
        "name": result.name,
        "runs": result.runs,
        "success_count": result.success_count,
        "success_rate": result.success_rate,
        "e2e_seconds": {"p50": result.p50_s, "p95": result.p95_s},
        "cancel_latency_ms": {"p50": result.cancel_p50_ms, "p95": result.cancel_p95_ms},
        "peak_total_rss_mb": result.peak_total_rss_mb,
        "peak_vram_gib": result.peak_vram_gib,
        "encoder_usage": result.encoder_usage,
        "failures": result.failures,
        "run_details": result.run_details,
    }


def evaluate_gate(baseline: ScenarioResult, resilience: Optional[ScenarioResult]) -> Dict[str, Any]:
    checks: List[Tuple[str, bool, str]] = []

    checks.append(("baseline_success_rate>=97%", baseline.success_rate >= 0.97, f"{baseline.success_rate:.3f}"))
    checks.append(("baseline_p50<=35s", baseline.p50_s is not None and baseline.p50_s <= 35.0, str(baseline.p50_s)))
    checks.append(("baseline_p95<=55s", baseline.p95_s is not None and baseline.p95_s <= 55.0, str(baseline.p95_s)))
    checks.append(
        (
            "cancel_p95<=300ms",
            baseline.cancel_p95_ms is not None and baseline.cancel_p95_ms <= 300.0,
            str(baseline.cancel_p95_ms),
        )
    )
    checks.append(
        (
            "peak_total_rss<=2048MB",
            baseline.peak_total_rss_mb is not None and baseline.peak_total_rss_mb <= 2048.0,
            str(baseline.peak_total_rss_mb),
        )
    )
    checks.append(
        (
            "peak_vram<=7.5GiB",
            baseline.peak_vram_gib is not None and baseline.peak_vram_gib <= 7.5,
            str(baseline.peak_vram_gib),
        )
    )

    if resilience is not None:
        checks.append(
            (
                "resilience_success_rate>=90%",
                resilience.success_rate >= 0.90,
                f"{resilience.success_rate:.3f}",
            )
        )

    failed = [name for name, ok, _ in checks if not ok]
    return {
        "passed": len(failed) == 0,
        "failed_checks": failed,
        "checks": [{"name": name, "ok": ok, "actual": actual} for name, ok, actual in checks],
    }


def run_scenario(
    name: str,
    provider: str,
    runs: int,
    fault_rate: float,
    python_bin: Path,
    server_dir: Path,
    base_output_dir: Path,
    port: int,
    nvenc_available: bool,
    http_max_retries: int,
    http_initial_backoff_ms: int,
    http_max_backoff_ms: int,
) -> ScenarioResult:
    env = os.environ.copy()
    env["STV_PROVIDER"] = provider
    env["STV_PORT"] = str(port)
    env["STV_HOST"] = "127.0.0.1"
    env["STV_OUTPUT_DIR"] = str(base_output_dir / name)
    env["STV_FAULT_INJECT_RATE"] = str(fault_rate)

    base_url = f"http://127.0.0.1:{port}"
    base_output_dir.mkdir(parents=True, exist_ok=True)
    (base_output_dir / name).mkdir(parents=True, exist_ok=True)

    server = ServerHandle(python_bin=python_bin, server_dir=server_dir, env=env)
    server.start()
    try:
        server.wait_healthy(base_url)

        peak_rss_mb = 0.0
        peak_vram_gib = 0.0
        vram_seen = False
        run_details: List[Dict[str, Any]] = []

        for i in range(1, runs + 1):
            detail = run_single_workflow(
                base_url=base_url,
                output_dir=base_output_dir / name,
                run_index=i,
                nvenc_available=nvenc_available,
                provider=provider,
                http_max_retries=http_max_retries,
                http_initial_backoff_ms=http_initial_backoff_ms,
                http_max_backoff_ms=http_max_backoff_ms,
            )
            detail["run_index"] = i

            cancel_latency = measure_cancel_latency(base_url, f"{name}-{i}")
            detail["cancel_latency_ms"] = cancel_latency

            rss_mb = read_total_rss_mb(server.proc.pid if server.proc else -1)
            if rss_mb is not None:
                peak_rss_mb = max(peak_rss_mb, rss_mb)

            vram_gib = read_peak_vram_gib()
            if vram_gib is not None:
                vram_seen = True
                peak_vram_gib = max(peak_vram_gib, vram_gib)

            run_details.append(detail)

        return summarize_scenario(
            name=name,
            run_details=run_details,
            peak_rss_mb=peak_rss_mb if peak_rss_mb > 0 else None,
            peak_vram_gib=peak_vram_gib if vram_seen else None,
        )
    finally:
        server.stop()


def write_report(
    report_path: Path,
    provider: str,
    baseline: ScenarioResult,
    resilience: Optional[ScenarioResult],
    gate: Dict[str, Any],
) -> None:
    report_path.parent.mkdir(parents=True, exist_ok=True)

    def fmt(v: Optional[float], digits: int = 2) -> str:
        if v is None:
            return "N/A"
        return f"{v:.{digits}f}"

    lines = [
        "# M2 Performance Report",
        "",
        f"- Generated at: {now_iso()}",
        f"- Provider: `{provider}`",
        f"- Baseline runs: `{baseline.runs}`",
        f"- Gate result: `{'PASS' if gate['passed'] else 'FAIL'}`",
        "",
        "## Baseline Metrics",
        "",
        f"- Success rate: `{baseline.success_rate * 100:.2f}%`",
        f"- E2E latency: `P50={fmt(baseline.p50_s)}s`, `P95={fmt(baseline.p95_s)}s`",
        f"- Cancel latency: `P50={fmt(baseline.cancel_p50_ms)}ms`, `P95={fmt(baseline.cancel_p95_ms)}ms`",
        f"- Peak total RSS: `{fmt(baseline.peak_total_rss_mb)} MB`",
        f"- Peak VRAM: `{fmt(baseline.peak_vram_gib)} GiB`",
        f"- Encoder usage: `{baseline.encoder_usage}`",
        "",
    ]

    if resilience is not None:
        lines += [
            "## Resilience Metrics (Injected 5xx)",
            "",
            f"- Success rate: `{resilience.success_rate * 100:.2f}%`",
            f"- E2E latency: `P50={fmt(resilience.p50_s)}s`, `P95={fmt(resilience.p95_s)}s`",
            f"- Cancel latency: `P50={fmt(resilience.cancel_p50_ms)}ms`, `P95={fmt(resilience.cancel_p95_ms)}ms`",
            "",
        ]

    lines += [
        "## Gate Checks",
        "",
        "| Check | Result | Actual |",
        "|---|---|---|",
    ]
    for check in gate["checks"]:
        lines.append(
            f"| {check['name']} | {'PASS' if check['ok'] else 'FAIL'} | {check['actual']} |"
        )

    if gate["failed_checks"]:
        lines += ["", "## Failed Checks", ""]
        for item in gate["failed_checks"]:
            lines.append(f"- {item}")

    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run StoryToVideo M2 benchmark.")
    parser.add_argument("--provider", choices=["mock", "local_gpu"], required=True)
    parser.add_argument("--runs", type=int, default=30, help="Number of baseline runs.")
    parser.add_argument("--out", default="bench.json", help="Output JSON path.")
    parser.add_argument(
        "--report",
        default="docs/reports/M2_perf.md",
        help="Output markdown report path.",
    )
    parser.add_argument("--port", type=int, default=8768, help="Server port for benchmark run.")
    parser.add_argument(
        "--resilience-runs",
        type=int,
        default=30,
        help="Number of runs for injected-fault scenario.",
    )
    parser.add_argument(
        "--resilience-fault-rate",
        type=float,
        default=0.30,
        help="Fault injection rate for resilience scenario.",
    )
    parser.add_argument(
        "--skip-resilience",
        action="store_true",
        help="Skip the 5xx-injection resilience scenario.",
    )
    parser.add_argument(
        "--server-python",
        default="server/venv/bin/python",
        help="Python executable used to launch FastAPI server.",
    )
    parser.add_argument(
        "--http-max-retries",
        type=int,
        default=4,
        help="Per-request retry count for retryable HTTP failures (5xx/429/network).",
    )
    parser.add_argument(
        "--http-initial-backoff-ms",
        type=int,
        default=200,
        help="Initial backoff in milliseconds for HTTP retries.",
    )
    parser.add_argument(
        "--http-max-backoff-ms",
        type=int,
        default=2000,
        help="Maximum backoff in milliseconds for HTTP retries.",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    server_dir = repo_root / "server"
    output_json = (repo_root / args.out).resolve()
    output_report = (repo_root / args.report).resolve()
    output_dir = Path("/tmp/stv-bench-output")
    python_arg_path = Path(args.server_python).expanduser()
    if python_arg_path.is_absolute():
        python_bin = python_arg_path.absolute()
    else:
        python_bin = (repo_root / python_arg_path).absolute()
    if not python_bin.exists():
        print(f"error: server python not found: {python_bin}", file=sys.stderr)
        return 2

    if args.runs <= 0:
        print("error: --runs must be > 0", file=sys.stderr)
        return 2

    nvenc_available = ffmpeg_has_nvenc()
    try:
        baseline = run_scenario(
            name="baseline",
            provider=args.provider,
            runs=args.runs,
            fault_rate=0.0,
            python_bin=python_bin,
            server_dir=server_dir,
            base_output_dir=output_dir,
            port=args.port,
            nvenc_available=nvenc_available,
            http_max_retries=args.http_max_retries,
            http_initial_backoff_ms=args.http_initial_backoff_ms,
            http_max_backoff_ms=args.http_max_backoff_ms,
        )

        resilience: Optional[ScenarioResult] = None
        if not args.skip_resilience:
            resilience = run_scenario(
                name="resilience",
                provider=args.provider,
                runs=args.resilience_runs,
                fault_rate=args.resilience_fault_rate,
                python_bin=python_bin,
                server_dir=server_dir,
                base_output_dir=output_dir,
                port=args.port + 1,
                nvenc_available=nvenc_available,
                http_max_retries=args.http_max_retries,
                http_initial_backoff_ms=args.http_initial_backoff_ms,
                http_max_backoff_ms=args.http_max_backoff_ms,
            )
    except RuntimeError as exc:
        print("error: benchmark run failed before completion", file=sys.stderr)
        print(str(exc), file=sys.stderr)
        print(
            "hint: ensure server venv has dependencies installed:\n"
            "  cd server && source venv/bin/activate && pip install -r requirements.txt",
            file=sys.stderr,
        )
        return 1

    gate = evaluate_gate(baseline, resilience)
    payload = {
        "generated_at": now_iso(),
        "provider": args.provider,
        "nvenc_available": nvenc_available,
        "baseline": scenario_to_dict(baseline),
        "resilience": scenario_to_dict(resilience) if resilience is not None else None,
        "gate": gate,
    }

    output_json.parent.mkdir(parents=True, exist_ok=True)
    output_json.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    write_report(output_report, args.provider, baseline, resilience, gate)

    print(f"Benchmark JSON written to: {output_json}")
    print(f"Benchmark report written to: {output_report}")
    print(f"Gate: {'PASS' if gate['passed'] else 'FAIL'}")
    if gate["failed_checks"]:
        print("Failed checks:")
        for item in gate["failed_checks"]:
            print(f"  - {item}")

    return 0 if gate["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
