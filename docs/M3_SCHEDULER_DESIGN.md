# M3 Scheduler Design

## Overview

M3 upgrades the scheduler from M1 `SimpleScheduler` (single-thread tick loop) to a
`ThreadPoolScheduler` with:

- Dependency DAG wakeup (no full-table dependency scan)
- Aging-based anti-starvation priority scheduling
- Resource budget gating (CPU hard, RAM/VRAM soft)
- Cooperative pause/resume/cancel semantics under concurrency
- Runtime fallback to `simple` scheduler (`STV_SCHEDULER=simple`)

## Public API Changes

- `IScheduler::submit(...)` now returns `Result<void, TaskError>`.
- `TaskDescriptor` adds:
  - `ResourceDemand resource_demand`
  - `std::optional<TaskState> paused_from`
- `WorkflowEngine::start_workflow(...)` now returns `Result<std::string, TaskError>`.
- New scheduler config types:
  - `ResourceBudget`
  - `AgingPolicy`
  - `PausePolicy`
  - `SchedulerConfig`
- New factory:
  - `create_thread_pool_scheduler(const SchedulerConfig&, std::shared_ptr<ILogger>)`

## Scheduling Algorithm

### Submit

1. Validate `task_id`, stage pointer, and duplicate IDs.
2. Enforce strict dependency mode:
   - all deps must exist when submitted
   - missing dep => submit error
3. Validate CPU hard gate feasibility:
   - `task.cpu_slots > cpu_slots_hard` => submit error
4. Build DAG edges (`dep -> successor`) and initialize `unmet_deps`.
5. State assignment:
   - `unmet_deps == 0` => `Ready`
   - otherwise `Queued`

### Worker Dispatch

- Worker threads only pick runnable `Ready` tasks.
- Candidate scoring:

`effective_priority = base_priority + floor(wait_ms / aging_interval_ms) * aging_boost`

- Tie-breakers:
  1. earlier `ready_since`
  2. lexical task_id

### Resource Budget

- CPU slots: hard cap (`running_cpu + demand_cpu <= cpu_slots_hard`)
- RAM/VRAM: soft cap (`running + demand <= soft_limit`) normally required
- Escape rule:
  - when no tasks are running, one soft-over-budget task may run (prevents permanent starvation)

## DAG Wakeup / Failure Propagation

- On task success:
  - decrement `unmet_deps` for direct successors only
  - successors whose `unmet_deps == 0` transition to `Ready`
- On task failure/cancel:
  - descendants are marked dependency-canceled to avoid indefinite pending tasks

## Pause / Resume / Cancel Semantics

- `pause(task_id)`:
  - `Queued` / `Ready`: immediate `Paused`
  - `Running`: set pause request and wait for progress checkpoint
  - checkpoint timeout => auto-cancel task and return timeout error
- `resume(task_id)`:
  - restore from `paused_from` (`Queued`, `Ready`, or `Running`)
- `cancel(task_id)`:
  - idempotent on already-canceled tasks
  - cancels token for running task and propagates dependency cancellation downstream

## Complexity

- Submit: `O(dep_count + cycle_guard)`
- Dispatch selection: `O(|ready_set|)`
- Success wakeup: `O(out_degree)`
- Failure propagation: `O(reachable_descendants)`

## Rollout and Fallback

- Default: `threadpool`
- Fallback: `simple`
- Env toggles:
  - `STV_SCHEDULER=threadpool|simple`
  - `STV_SCHED_WORKERS`
  - `STV_SCHED_CPU_SLOTS`
  - `STV_SCHED_RAM_MB_SOFT`
  - `STV_SCHED_VRAM_MB_SOFT`
  - `STV_SCHED_AGING_INTERVAL_MS`
  - `STV_SCHED_AGING_BOOST`
  - `STV_SCHED_PAUSE_TIMEOUT_MS`

## Validation Targets

- New tests:
  - `test_thread_pool_scheduler`
  - `test_orchestrator`
  - updated `test_task_state_machine`
- Benchmark entry:
  - `scripts/bench_m3.py --scheduler simple|threadpool --runs N --out <json>`
