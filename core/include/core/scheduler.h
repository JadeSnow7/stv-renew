#pragma once

#include "core/pipeline.h"
#include "core/result.h"
#include "core/task.h"
#include "core/task_error.h"

#include <functional>
#include <memory>
#include <string>

namespace stv::core {

class ILogger;

/// Scheduler resource budget (M3).
/// CPU is a hard gate; RAM/VRAM are soft gates.
struct ResourceBudget {
  int cpu_slots_hard = 0; // 0 = auto (equal to worker_count)
  int ram_soft_mb = 2048;
  int vram_soft_mb = 7680;
};

/// Priority aging policy for anti-starvation (M3).
struct AgingPolicy {
  int interval_ms = 500;
  int boost_per_interval = 1;
};

/// Pause policy for cooperative pause checkpoints (M3).
struct PausePolicy {
  int checkpoint_timeout_ms = 1500;
};

/// Scheduler runtime configuration (M3).
struct SchedulerConfig {
  int worker_count = 0; // 0 = auto: clamp((hw_threads - 1), 2, 8)
  ResourceBudget resource_budget{};
  AgingPolicy aging_policy{};
  PausePolicy pause_policy{};
};

/// Scheduler interface — manages task lifecycle and dispatch.
///
/// Design rationale (interview talking point):
/// We use callback-driven notification instead of future::get() blocking
/// because:
/// 1. Threads are not occupied while waiting for dependencies
/// 2. Supports pause/resume/cancel without thread interruption
/// 3. Dependency chains don't require nested blocking
///
/// M1 implementation: SimpleScheduler (single-thread, sequential execution).
/// M3 upgrade: ThreadPoolScheduler with dependency graph + priority queue +
/// resource budget.
class IScheduler {
public:
  virtual ~IScheduler() = default;

  /// Submit a task with its associated stage for execution.
  /// The scheduler owns the task lifecycle from this point.
  virtual Result<void, TaskError> submit(
      TaskDescriptor task, std::shared_ptr<IStage> stage) = 0;

  /// Request cancellation of a task.
  virtual Result<void, TaskError> cancel(const std::string &task_id) = 0;

  /// Pause a running task (M3: cooperative pause at next checkpoint).
  virtual Result<void, TaskError> pause(const std::string &task_id) = 0;

  /// Resume a paused task.
  virtual Result<void, TaskError> resume(const std::string &task_id) = 0;

  /// Callback type for state change notifications.
  /// Parameters: task_id, new state, progress [0,1]
  using StateCallback = std::function<void(
      const std::string &task_id, TaskState new_state, float progress)>;

  /// Register a callback for task state changes.
  /// The callback is invoked from the scheduler's execution context.
  virtual void on_state_change(StateCallback cb) = 0;

  /// Process pending tasks. Call from event loop or timer.
  /// For SimpleScheduler: executes one ready task per call.
  virtual void tick() = 0;

  /// Check if there are any non-terminal tasks.
  [[nodiscard]] virtual bool has_pending_tasks() const = 0;
};

/// M1 scheduler: single-threaded tick-based fallback implementation.
std::unique_ptr<IScheduler> create_simple_scheduler();

/// M3 scheduler: thread-pool + DAG + budget-aware dispatch.
std::unique_ptr<IScheduler>
create_thread_pool_scheduler(const SchedulerConfig &config,
                             std::shared_ptr<ILogger> logger);

} // namespace stv::core
