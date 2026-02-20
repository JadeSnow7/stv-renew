# M2 Performance Report

- Generated at: 2026-02-20T11:26:39.371777+00:00
- Provider: `local_gpu`
- Baseline runs: `30`
- Gate result: `PASS`

## Baseline Metrics

- Success rate: `100.00%`
- E2E latency: `P50=2.94s`, `P95=3.30s`
- Cancel latency: `P50=0.95ms`, `P95=1.10ms`
- Peak total RSS: `85.80 MB`
- Peak VRAM: `0.01 GiB`
- Encoder usage: `{'h264_nvenc': 30, 'libx264': 0}`

## Resilience Metrics (Injected 5xx)

- Success rate: `93.33%`
- E2E latency: `P50=4.32s`, `P95=6.65s`
- Cancel latency: `P50=1.07ms`, `P95=1.24ms`

## Gate Checks

| Check | Result | Actual |
|---|---|---|
| baseline_success_rate>=97% | PASS | 1.000 |
| baseline_p50<=35s | PASS | 2.9443758684992645 |
| baseline_p95<=55s | PASS | 3.3013201997493526 |
| cancel_p95<=300ms | PASS | 1.1007291500391145 |
| peak_total_rss<=2048MB | PASS | 85.796875 |
| peak_vram<=7.5GiB | PASS | 0.0107421875 |
| resilience_success_rate>=90% | PASS | 0.933 |
