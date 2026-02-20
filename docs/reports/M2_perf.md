# M2 Performance Report (Mock + Local GPU Mode)

- Generated at: 2026-02-20 (local execution)
- Baseline run count: 30
- Resilience run count: 30 (`STV_FAULT_INJECT_RATE=0.30`)

## 1) Mock Provider (`bench.json`)

- Gate: **PASS**
- Success rate: **100.00%**
- E2E latency: **P50 3.30s / P95 3.43s**
- Cancel latency: **P50 153.77ms / P95 154.75ms**
- Peak RSS (client+server): **83.87 MB**
- Peak VRAM: **0.01 GiB**
- Encoder usage: **libx264 30 / h264_nvenc 0**
- Resilience success rate (30% injected 5xx): **96.67%**

## 2) Local GPU Mode (`bench_gpu.json`)

- Gate: **PASS**
- Success rate: **100.00%**
- E2E latency: **P50 2.94s / P95 3.30s**
- Cancel latency: **P50 0.95ms / P95 1.10ms**
- Peak RSS (client+server): **85.80 MB**
- Peak VRAM: **0.01 GiB**
- Encoder usage: **h264_nvenc 30 / libx264 0**
- Resilience success rate (30% injected 5xx): **93.33%**

## Gate Checks

- Baseline success rate `>=97%`: PASS
- Baseline E2E `P50<=35s`, `P95<=55s`: PASS
- Cancel latency `P95<=300ms`: PASS
- Peak RSS `<=2048MB`: PASS
- Peak VRAM `<=7.5GiB`: PASS
- Resilience success rate `>=90%`: PASS

## Important Note

Current host did **not** install `requirements-gpu.txt` (`torch/diffusers` missing), so `local_gpu` provider validated the **degrade/fallback path** (mock image generation + ffmpeg/NVENC path), not true SD GPU inference quality/perf.  
To complete strict real-GPU inference acceptance, install GPU dependencies and rerun:

```bash
cd server
source venv/bin/activate
pip install -r requirements-gpu.txt
cd ..
./scripts/bench_m2.py --provider local_gpu --runs 30 --resilience-runs 30 --out bench_gpu.json --report docs/reports/M2_perf_gpu.md
```
