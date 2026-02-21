# M3 Scheduler Performance Report

## Environment

- Date: 2026-02-21
- Benchmark tool: `scripts/bench_m3.py`
- Workload: `scripts/test_e2e_mock.sh` (4-scene mock workflow)
- Sample size: `1` run per scheduler mode (smoke-level only)

## Raw Results

| Scheduler | Runs | Success Rate | P50 (s) | P95 (s) |
|---|---:|---:|---:|---:|
| simple | 1 | 100% | 4.7546 | 4.7546 |
| threadpool | 1 | 100% | 4.7477 | 4.7477 |

## Derived Comparison

- ThreadPool vs Simple P50 delta: **+0.14% faster** (`(4.7546 - 4.7477) / 4.7546`)
- Current data does **not** satisfy the M3 acceptance target (`>=25%` P50 improvement), but this run is only a 1-sample smoke check.

## Queue/Budget Metrics

- Queue wait extraction: unavailable from current E2E log format
- Resource budget hit extraction: unavailable from current E2E log format
- Functional coverage for these behaviors is provided by unit tests in `tests/test_thread_pool_scheduler.cpp`.

## Gate Status (Preliminary)

- Build/Test gate: ✅ `scripts/test_build.sh` (69/69 tests pass)
- ThreadPool behavior gate: ✅ unit tests pass (deps/aging/budget/pause/cancel/race)
- Performance gate: ⚠️ **pending full benchmark campaign**

## Next Action for Final M3 Perf Gate

Run larger samples on the target machine:

```bash
./scripts/bench_m3.py --scheduler simple --runs 30 --out docs/reports/m3_simple.json
./scripts/bench_m3.py --scheduler threadpool --runs 30 --out docs/reports/m3_threadpool.json
```

Then recompute:

- P50/P95 improvement
- Starvation bound (`<= 3 * aging_interval + single-task upper bound`)
- Throughput deltas
