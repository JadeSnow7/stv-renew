#!/usr/bin/env python3
"""M3 scheduler benchmark runner.

Runs the existing mock E2E script repeatedly with a selected scheduler backend,
then emits a structured JSON report for p50/p95 latency, throughput, success
rate, and any queue/budget hints found in logs.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import re
import statistics
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

QUEUE_WAIT_RE = re.compile(r"queue_wait_ms=(\d+)")
BUDGET_HIT_RE = re.compile(r"budget_hit=(\w+)")


def percentile(values: list[float], p: float) -> float:
    if not values:
        return 0.0
    sorted_values = sorted(values)
    rank = (len(sorted_values) - 1) * p
    lower = int(rank)
    upper = min(lower + 1, len(sorted_values) - 1)
    if lower == upper:
        return sorted_values[lower]
    frac = rank - lower
    return sorted_values[lower] * (1.0 - frac) + sorted_values[upper] * frac


def run_once(command: list[str], scheduler: str, cwd: Path) -> dict[str, Any]:
    env = os.environ.copy()
    env["STV_SCHEDULER"] = scheduler

    started = time.perf_counter()
    proc = subprocess.run(
        command,
        cwd=str(cwd),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    elapsed = time.perf_counter() - started

    output = (proc.stdout or "") + "\n" + (proc.stderr or "")
    queue_wait_ms = [int(v) for v in QUEUE_WAIT_RE.findall(output)]
    budget_hits = BUDGET_HIT_RE.findall(output)

    return {
        "ok": proc.returncode == 0,
        "returncode": proc.returncode,
        "elapsed_sec": elapsed,
        "queue_wait_ms": queue_wait_ms,
        "budget_hits": budget_hits,
    }


def build_report(
    scheduler: str,
    runs: list[dict[str, Any]],
    scene_count: int,
    command: list[str],
) -> dict[str, Any]:
    successful = [r for r in runs if r["ok"]]
    latencies = [r["elapsed_sec"] for r in successful]

    throughput = []
    for latency in latencies:
        if latency > 0:
            throughput.append(scene_count / latency)

    queue_wait_samples = [
        value for run in runs for value in run.get("queue_wait_ms", [])
    ]
    budget_hits_total = sum(len(run.get("budget_hits", [])) for run in runs)

    return {
        "generated_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "scheduler": scheduler,
        "command": command,
        "runs": len(runs),
        "success": len(successful),
        "failure": len(runs) - len(successful),
        "success_rate": (len(successful) / len(runs)) if runs else 0.0,
        "latency_sec": {
            "p50": percentile(latencies, 0.50) if latencies else None,
            "p95": percentile(latencies, 0.95) if latencies else None,
            "mean": statistics.mean(latencies) if latencies else None,
            "samples": latencies,
        },
        "throughput_scenes_per_sec": {
            "mean": statistics.mean(throughput) if throughput else None,
            "samples": throughput,
        },
        "queue_wait_ms": {
            "available": bool(queue_wait_samples),
            "avg": statistics.mean(queue_wait_samples) if queue_wait_samples else None,
            "samples": queue_wait_samples,
        },
        "resource_budget_hits": {
            "total": budget_hits_total,
        },
        "raw_runs": runs,
    }


def write_markdown_report(path: Path, report: dict[str, Any]) -> None:
    latency = report["latency_sec"]
    queue_wait = report["queue_wait_ms"]
    throughput = report["throughput_scenes_per_sec"]

    lines = [
        "# M3 Scheduler Performance Report",
        "",
        f"- Generated at: `{report['generated_at']}`",
        f"- Scheduler: `{report['scheduler']}`",
        f"- Runs: `{report['runs']}`",
        f"- Success rate: `{report['success_rate'] * 100:.2f}%`",
        "",
        "## Latency",
        "",
        f"- P50: `{latency['p50']}` sec",
        f"- P95: `{latency['p95']}` sec",
        f"- Mean: `{latency['mean']}` sec",
        "",
        "## Throughput",
        "",
        f"- Mean scenes/sec: `{throughput['mean']}`",
        "",
        "## Queue Wait / Budget",
        "",
        f"- Queue wait available: `{queue_wait['available']}`",
        f"- Queue wait avg: `{queue_wait['avg']}` ms",
        f"- Resource budget hit count: `{report['resource_budget_hits']['total']}`",
        "",
        "## Acceptance Notes",
        "",
        "- Compare this report with the `simple` scheduler baseline to verify the `>=25%` P50 improvement target.",
        "- Validate starvation bound with dedicated aging stress tests in `test_thread_pool_scheduler`.",
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run M3 scheduler benchmark")
    parser.add_argument("--scheduler", choices=["simple", "threadpool"], required=True)
    parser.add_argument("--runs", type=int, default=10)
    parser.add_argument("--out", required=True, help="JSON output path")
    parser.add_argument(
        "--command",
        default="./scripts/test_e2e_mock.sh",
        help="Command used for each benchmark run",
    )
    parser.add_argument("--scene-count", type=int, default=4)
    parser.add_argument("--report", help="Optional markdown report output path")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.runs <= 0:
        print("--runs must be > 0", file=sys.stderr)
        return 2

    root = Path(__file__).resolve().parent.parent
    command = args.command.split()

    runs: list[dict[str, Any]] = []
    for idx in range(args.runs):
        print(f"[bench_m3] run {idx + 1}/{args.runs} scheduler={args.scheduler}")
        result = run_once(command, args.scheduler, root)
        runs.append(result)
        print(
            f"  -> rc={result['returncode']} elapsed={result['elapsed_sec']:.3f}s "
            f"ok={result['ok']}"
        )

    report = build_report(args.scheduler, runs, args.scene_count, command)

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(report, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")
    print(f"[bench_m3] wrote {out_path}")

    if args.report:
        report_path = Path(args.report)
        write_markdown_report(report_path, report)
        print(f"[bench_m3] wrote {report_path}")

    return 0 if report["success"] > 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
