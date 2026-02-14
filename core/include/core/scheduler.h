#pragma once

#include "core/pipeline.h"
#include "core/result.h"
#include "core/task.h"
#include "core/task_error.h"

#include <functional>
#include <memory>
#include <string>

namespace stv::core {

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
  virtual void submit(TaskDescriptor task, std::shared_ptr<IStage> stage) = 0;

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

} // namespace stv::core
