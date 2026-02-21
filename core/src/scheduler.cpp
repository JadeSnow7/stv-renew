#include "core/scheduler.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace stv::core {

/// SimpleScheduler — M1 implementation.
/// Single-threaded, sequential execution driven by tick().
/// Each tick() picks one Ready task (highest priority) and executes it
/// synchronously.
///
/// Interview talking point:
/// "M1 uses a simple tick-based scheduler to prove the architecture.
///  M3 replaces it with a thread-pool scheduler that maintains the same
///  IScheduler interface, adding dependency graph resolution, priority queue,
///  and resource budgets."
class SimpleScheduler : public IScheduler {
public:
  Result<void, TaskError> submit(TaskDescriptor task,
                                 std::shared_ptr<IStage> stage) override {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!stage) {
      return Result<void, TaskError>::Err(
          TaskError::Internal("Stage must not be null"));
    }

    if (task.task_id.empty()) {
      return Result<void, TaskError>::Err(
          TaskError::Internal("task_id must not be empty"));
    }

    const std::string id = task.task_id;
    if (find_entry(id) != entries_.end()) {
      return Result<void, TaskError>::Err(
          TaskError::Internal("Duplicate task_id: " + id));
    }

    for (const auto &dep_id : task.deps) {
      if (dep_id == id) {
        return Result<void, TaskError>::Err(
            TaskError::Internal("Task cannot depend on itself: " + id));
      }
      if (find_entry(dep_id) == entries_.end()) {
        return Result<void, TaskError>::Err(
            TaskError::Internal("Dependency not found: " + dep_id));
      }
    }

    // Check if dependencies are already met (empty deps → Ready immediately)
    if (task.deps.empty()) {
      auto to_ready = task.transition_to(TaskState::Ready);
      if (to_ready.is_err()) {
        return Result<void, TaskError>::Err(to_ready.error());
      }
    }

    entries_.push_back(Entry{std::move(task), std::move(stage), {}});
    return Result<void, TaskError>::Ok();
  }

  Result<void, TaskError> cancel(const std::string &task_id) override {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = find_entry(task_id);
    if (it == entries_.end()) {
      return Result<void, TaskError>::Err(
          TaskError::Internal("Task not found: " + task_id));
    }
    if (it->task.cancel_token) {
      it->task.cancel_token->request_cancel();
    }
    auto result = it->task.transition_to(TaskState::Canceled);
    if (result.is_ok()) {
      notify(task_id, TaskState::Canceled, it->task.progress);
    }
    return result;
  }

  Result<void, TaskError> pause(const std::string &task_id) override {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = find_entry(task_id);
    if (it == entries_.end()) {
      return Result<void, TaskError>::Err(
          TaskError::Internal("Task not found: " + task_id));
    }
    auto result = it->task.transition_to(TaskState::Paused);
    if (result.is_ok()) {
      notify(task_id, TaskState::Paused, it->task.progress);
    }
    return result;
  }

  Result<void, TaskError> resume(const std::string &task_id) override {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = find_entry(task_id);
    if (it == entries_.end()) {
      return Result<void, TaskError>::Err(
          TaskError::Internal("Task not found: " + task_id));
    }
    const TaskState resume_target = it->task.paused_from.value_or(TaskState::Running);
    auto result = it->task.transition_to(resume_target);
    if (result.is_ok()) {
      notify(task_id, resume_target, it->task.progress);
    }
    return result;
  }

  void on_state_change(StateCallback cb) override {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(std::move(cb));
  }

  void tick() override {
    std::unique_lock<std::mutex> lock(mutex_);

    // First: promote Queued tasks whose deps are all Succeeded
    for (auto &entry : entries_) {
      if (entry.task.state == TaskState::Queued && !entry.task.deps.empty()) {
        bool all_deps_met = true;
        for (const auto &dep_id : entry.task.deps) {
          auto dep_it = find_entry(dep_id);
          if (dep_it == entries_.end() ||
              dep_it->task.state != TaskState::Succeeded) {
            all_deps_met = false;
            break;
          }
        }
        if (all_deps_met) {
          entry.task.transition_to(TaskState::Ready);
          notify(entry.task.task_id, TaskState::Ready, 0.0f);
        }
      }
    }

    // Pick highest-priority Ready task
    Entry *best = nullptr;
    for (auto &entry : entries_) {
      if (entry.task.state == TaskState::Ready) {
        if (!best || entry.task.priority > best->task.priority) {
          best = &entry;
        }
      }
    }

    if (!best)
      return; // Nothing to execute

    // Transition to Running
    best->task.transition_to(TaskState::Running);
    const std::string task_id = best->task.task_id;
    notify(task_id, TaskState::Running, 0.0f);

    // Set up progress reporting
    auto stage = best->stage;
    StageContext ctx;
    ctx.trace_id = best->task.trace_id;
    ctx.cancel_token = best->task.cancel_token;
    ctx.on_progress = [this, &task_id](float p) {
      // Note: this is called from within execute(), potentially
      // under lock — but SimpleScheduler is single-threaded so no deadlock.
      auto it = find_entry(task_id);
      if (it != entries_.end()) {
        it->task.set_progress(p);
        notify(task_id, TaskState::Running, p);
      }
    };

    // Transfer outputs from predecessor tasks as inputs
    if (best->task.deps.size() > 0) {
      for (const auto &dep_id : best->task.deps) {
        auto dep_it = find_entry(dep_id);
        if (dep_it != entries_.end()) {
          for (const auto &[key, val] : dep_it->last_outputs) {
            ctx.inputs[key] = val;
          }
        }
      }
    }

    // Release lock during execution (allows cancel from another thread)
    lock.unlock();

    // Execute the stage
    auto result = stage->execute(ctx);

    lock.lock();
    auto it = find_entry(task_id);
    if (it == entries_.end())
      return; // removed during execution?

    if (result.is_ok()) {
      it->task.transition_to(TaskState::Succeeded);
      it->task.set_progress(1.0f);
      it->last_outputs = ctx.outputs; // Save outputs for dependents
      notify(task_id, TaskState::Succeeded, 1.0f);
    } else {
      const auto &err = result.error();
      it->task.error = err;
      if (err.category == ErrorCategory::Canceled) {
        it->task.transition_to(TaskState::Canceled);
        notify(task_id, TaskState::Canceled, it->task.progress);
      } else {
        it->task.transition_to(TaskState::Failed);
        notify(task_id, TaskState::Failed, it->task.progress);
      }
    }
  }

  [[nodiscard]] bool has_pending_tasks() const override {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::any_of(entries_.begin(), entries_.end(), [](const Entry &e) {
      return !is_terminal(e.task.state);
    });
  }

private:
  struct Entry {
    TaskDescriptor task;
    std::shared_ptr<IStage> stage;
    std::unordered_map<std::string, std::any> last_outputs;
  };

  mutable std::mutex mutex_;
  std::vector<Entry> entries_;
  std::vector<StateCallback> callbacks_;

  std::vector<Entry>::iterator find_entry(const std::string &task_id) {
    return std::find_if(
        entries_.begin(), entries_.end(),
        [&task_id](const Entry &e) { return e.task.task_id == task_id; });
  }

  void notify(const std::string &task_id, TaskState state, float progress) {
    for (auto &cb : callbacks_) {
      if (cb)
        cb(task_id, state, progress);
    }
  }
};

// Factory function to create default scheduler
std::unique_ptr<IScheduler> create_simple_scheduler() {
  return std::make_unique<SimpleScheduler>();
}

} // namespace stv::core
