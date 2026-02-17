# Performance Baseline (Phase 2)

## Target Metrics

1. Startup latency (cold start p95).
2. Idle memory footprint.
3. SSE reconnect recovery time.
4. Cancel acknowledgement latency.

## Method

1. Use fixed hardware profiles per OS.
2. Run 20 samples each, discard warm-up outliers.
3. Compare against previous release baseline.

## Tools

- Windows: WPA / ETW / Visual Studio Profiler
- macOS: Instruments
- Linux: perf / flamegraph
