#include "core/scheduler.h"

#include "core/logger.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace stv::core {
namespace {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

int clamp_auto_workers() {
  const auto hw = static_cast<int>(std::thread::hardware_concurrency());
  if (hw <= 0) {
    return 4;
  }
  return std::clamp(hw - 1, 2, 8);
}

struct StateEvent {
  std::string task_id;
  TaskState state;
  float progress;
};

struct ResourceUsage {
  int cpu_slots = 0;
  int ram_mb = 0;
  int vram_mb = 0;
};

class ThreadPoolScheduler final : public IScheduler {
public:
  ThreadPoolScheduler(SchedulerConfig config, std::shared_ptr<ILogger> logger)
      : config_(normalize_config(std::move(config))), logger_(std::move(logger)) {
    workers_.reserve(static_cast<size_t>(config_.worker_count));
    for (int i = 0; i < config_.worker_count; ++i) {
      workers_.emplace_back([this]() { worker_loop(); });
    }
  }

  ~ThreadPoolScheduler() override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopping_ = true;
    }
    cv_.notify_all();
    for (auto &w : workers_) {
      if (w.joinable()) {
        w.join();
      }
    }
  }

  Result<void, TaskError> submit(TaskDescriptor task,
                                 std::shared_ptr<IStage> stage) override {
    std::vector<StateEvent> events;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!stage) {
        return Result<void, TaskError>::Err(
            TaskError::Internal("Stage must not be null"));
      }
      if (task.task_id.empty()) {
        return Result<void, TaskError>::Err(
            TaskError::Internal("task_id must not be empty"));
      }
      if (nodes_.find(task.task_id) != nodes_.end()) {
        return Result<void, TaskError>::Err(
            TaskError::Internal("Duplicate task_id: " + task.task_id));
      }
      if (task.resource_demand.cpu_slots <= 0) {
        task.resource_demand.cpu_slots = 1;
      }
      task.resource_demand.ram_mb = std::max(0, task.resource_demand.ram_mb);
      task.resource_demand.vram_mb = std::max(0, task.resource_demand.vram_mb);
      if (task.resource_demand.cpu_slots >
          config_.resource_budget.cpu_slots_hard) {
        return Result<void, TaskError>::Err(TaskError(
            ErrorCategory::Resource, 3001, false,
            "Task requires too many CPU slots",
            "resource_demand.cpu_slots exceeds hard CPU budget", {
                {"task_id", task.task_id},
                {"cpu_slots", std::to_string(task.resource_demand.cpu_slots)},
                {"cpu_slots_hard",
                 std::to_string(config_.resource_budget.cpu_slots_hard)},
            }));
      }
      if (!task.cancel_token) {
        task.cancel_token = CancelToken::create();
      }

      if (creates_cycle_locked(task.task_id, task.deps)) {
        return Result<void, TaskError>::Err(TaskError::Internal(
            "Dependency cycle detected for task: " + task.task_id));
      }

      Node node;
      node.task = std::move(task);
      node.stage = std::move(stage);
      node.unmet_deps = 0;

      bool dependency_blocked = false;
      std::string blocked_dep_id;
      for (const auto &dep_id : node.task.deps) {
        if (dep_id == node.task.task_id) {
          return Result<void, TaskError>::Err(TaskError::Internal(
              "Task cannot depend on itself: " + node.task.task_id));
        }
        auto dep_it = nodes_.find(dep_id);
        if (dep_it == nodes_.end()) {
          return Result<void, TaskError>::Err(
              TaskError::Internal("Dependency not found: " + dep_id));
        }
        successors_[dep_id].push_back(node.task.task_id);

        const auto dep_state = dep_it->second.task.state;
        if (dep_state == TaskState::Succeeded) {
          continue;
        }
        if (dep_state == TaskState::Failed || dep_state == TaskState::Canceled) {
          dependency_blocked = true;
          blocked_dep_id = dep_id;
          break;
        }
        node.unmet_deps++;
      }

      if (dependency_blocked) {
        node.task.error = TaskError(
            ErrorCategory::Canceled, 3002, false,
            "Task canceled because dependency already failed",
            "Dependency already terminal before submit", {
                {"dependency_task_id", blocked_dep_id},
            });
        auto canceled = node.task.transition_to(TaskState::Canceled);
        if (canceled.is_ok()) {
          events.push_back({node.task.task_id, TaskState::Canceled,
                            node.task.progress});
        }
      } else if (node.unmet_deps == 0) {
        auto ready = node.task.transition_to(TaskState::Ready);
        if (ready.is_err()) {
          return Result<void, TaskError>::Err(ready.error());
        }
        node.ready_since = Clock::now();
        ready_set_.insert(node.task.task_id);
        events.push_back({node.task.task_id, TaskState::Ready, node.task.progress});
      }

      nodes_.emplace(node.task.task_id, std::move(node));
    }

    dispatch_events(events);
    cv_.notify_all();
    return Result<void, TaskError>::Ok();
  }

  Result<void, TaskError> cancel(const std::string &task_id) override {
    std::vector<StateEvent> events;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = nodes_.find(task_id);
      if (it == nodes_.end()) {
        return Result<void, TaskError>::Err(
            TaskError::Internal("Task not found: " + task_id));
      }

      auto &node = it->second;
      if (node.task.cancel_token) {
        node.task.cancel_token->request_cancel();
      }

      if (node.task.state == TaskState::Canceled) {
        node.pause_requested = false;
        node.pause_deadline.reset();
        cv_.notify_all();
        return Result<void, TaskError>::Ok();
      }

      if (node.task.state == TaskState::Ready) {
        ready_set_.erase(task_id);
      }

      node.pause_requested = false;
      node.pause_deadline.reset();

      bool should_propagate = false;
      if (!is_terminal(node.task.state)) {
        auto to_canceled = node.task.transition_to(TaskState::Canceled);
        if (to_canceled.is_err()) {
          return to_canceled;
        }
        if (!node.task.error.has_value()) {
          node.task.error = TaskError::Canceled();
        }
        events.push_back({task_id, TaskState::Canceled, node.task.progress});
        should_propagate = true;
      }

      if (should_propagate) {
        propagate_dependency_canceled_locked(task_id, events);
      }
    }

    dispatch_events(events);
    cv_.notify_all();
    return Result<void, TaskError>::Ok();
  }

  Result<void, TaskError> pause(const std::string &task_id) override {
    std::vector<StateEvent> events;
    bool timed_out = false;

    {
      std::unique_lock<std::mutex> lock(mutex_);
      auto it = nodes_.find(task_id);
      if (it == nodes_.end()) {
        return Result<void, TaskError>::Err(
            TaskError::Internal("Task not found: " + task_id));
      }

      auto &node = it->second;
      if (node.task.state == TaskState::Paused) {
        return Result<void, TaskError>::Ok();
      }

      if (node.task.state == TaskState::Queued || node.task.state == TaskState::Ready) {
        if (node.task.state == TaskState::Ready) {
          ready_set_.erase(task_id);
        }
        auto paused = node.task.transition_to(TaskState::Paused);
        if (paused.is_err()) {
          return paused;
        }
        events.push_back({task_id, TaskState::Paused, node.task.progress});
      } else if (node.task.state == TaskState::Running) {
        node.pause_requested = true;
        const auto timeout = std::max(1, config_.pause_policy.checkpoint_timeout_ms);
        node.pause_deadline = Clock::now() + std::chrono::milliseconds(timeout);

        const bool reached = cv_.wait_until(
            lock, *node.pause_deadline, [&]() {
              auto it2 = nodes_.find(task_id);
              if (it2 == nodes_.end()) {
                return true;
              }
              return it2->second.task.state == TaskState::Paused ||
                     it2->second.task.state == TaskState::Canceled ||
                     it2->second.task.state == TaskState::Failed ||
                     it2->second.task.state == TaskState::Succeeded;
            });

        if (!reached) {
          timed_out = true;
        }
      } else {
        return Result<void, TaskError>::Err(TaskError::Internal(
            "pause() only supports Queued/Ready/Running/Paused task states"));
      }
    }

    if (timed_out) {
      auto cancel_result = cancel(task_id);
      (void)cancel_result;
      return Result<void, TaskError>::Err(TaskError(
          ErrorCategory::Timeout, 3003, false,
          "Pause timed out and task was canceled",
          "Pause checkpoint timeout, auto-canceled task", {
              {"task_id", task_id},
          }));
    }

    dispatch_events(events);
    cv_.notify_all();
    return Result<void, TaskError>::Ok();
  }

  Result<void, TaskError> resume(const std::string &task_id) override {
    std::vector<StateEvent> events;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = nodes_.find(task_id);
      if (it == nodes_.end()) {
        return Result<void, TaskError>::Err(
            TaskError::Internal("Task not found: " + task_id));
      }

      auto &node = it->second;
      if (node.task.state != TaskState::Paused) {
        return Result<void, TaskError>::Err(
            TaskError::Internal("Task is not paused: " + task_id));
      }

      const TaskState target = node.task.paused_from.value_or(TaskState::Running);
      auto resumed = node.task.transition_to(target);
      if (resumed.is_err()) {
        return resumed;
      }

      node.pause_requested = false;
      node.pause_deadline.reset();
      if (target == TaskState::Ready) {
        node.ready_since = Clock::now();
        ready_set_.insert(task_id);
      }

      events.push_back({task_id, target, node.task.progress});
    }

    dispatch_events(events);
    cv_.notify_all();
    return Result<void, TaskError>::Ok();
  }

  void on_state_change(StateCallback cb) override {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(std::move(cb));
  }

  void tick() override {
    std::vector<std::string> timed_out_ids;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      const auto now = Clock::now();
      for (auto &[task_id, node] : nodes_) {
        if (node.task.state == TaskState::Running && node.pause_requested &&
            node.pause_deadline.has_value() && now >= *node.pause_deadline) {
          timed_out_ids.push_back(task_id);
        }
      }
    }

    for (const auto &task_id : timed_out_ids) {
      auto result = cancel(task_id);
      (void)result;
    }

    cv_.notify_all();
  }

  [[nodiscard]] bool has_pending_tasks() const override {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &[_, node] : nodes_) {
      if (!is_terminal(node.task.state)) {
        return true;
      }
    }
    return false;
  }

private:
  struct Node {
    TaskDescriptor task;
    std::shared_ptr<IStage> stage;
    std::unordered_map<std::string, std::any> last_outputs;
    size_t unmet_deps = 0;
    TimePoint ready_since = Clock::now();
    bool running = false;
    bool pause_requested = false;
    std::optional<TimePoint> pause_deadline;
  };

  struct Candidate {
    std::string task_id;
    long long effective_priority = std::numeric_limits<long long>::min();
    TimePoint ready_since{};
    bool soft_fit = true;
  };

  static SchedulerConfig normalize_config(SchedulerConfig config) {
    if (config.worker_count <= 0) {
      config.worker_count = clamp_auto_workers();
    }
    if (config.resource_budget.cpu_slots_hard <= 0) {
      config.resource_budget.cpu_slots_hard = config.worker_count;
    }
    config.resource_budget.ram_soft_mb =
        std::max(0, config.resource_budget.ram_soft_mb);
    config.resource_budget.vram_soft_mb =
        std::max(0, config.resource_budget.vram_soft_mb);

    if (config.aging_policy.interval_ms <= 0) {
      config.aging_policy.interval_ms = 500;
    }
    if (config.aging_policy.boost_per_interval <= 0) {
      config.aging_policy.boost_per_interval = 1;
    }
    if (config.pause_policy.checkpoint_timeout_ms <= 0) {
      config.pause_policy.checkpoint_timeout_ms = 1500;
    }
    return config;
  }

  void worker_loop() {
    while (true) {
      std::string task_id;
      std::shared_ptr<IStage> stage;
      StageContext ctx;
      std::vector<StateEvent> run_events;
      bool should_execute = false;

      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() {
          return stopping_ || has_runnable_task_locked();
        });

        if (stopping_) {
          return;
        }

        const auto candidate = pick_candidate_locked(/*allow_escape=*/true);
        if (!candidate.has_value()) {
          continue;
        }

        task_id = candidate->task_id;
        auto node_it = nodes_.find(task_id);
        if (node_it == nodes_.end()) {
          continue;
        }

        Node &node = node_it->second;
        ready_set_.erase(task_id);

        auto to_running = node.task.transition_to(TaskState::Running);
        if (to_running.is_err()) {
          node.task.error = to_running.error();
          auto to_failed = node.task.transition_to(TaskState::Failed);
          if (to_failed.is_ok()) {
            run_events.push_back({task_id, TaskState::Failed, node.task.progress});
          }
        } else {
          reserve_resources_locked(node.task.resource_demand);
          node.running = true;
          node.pause_requested = false;
          node.pause_deadline.reset();
          running_set_.insert(task_id);

          ctx.trace_id = node.task.trace_id;
          ctx.cancel_token = node.task.cancel_token;
          for (const auto &dep_id : node.task.deps) {
            auto dep_it = nodes_.find(dep_id);
            if (dep_it == nodes_.end()) {
              continue;
            }
            for (const auto &[key, value] : dep_it->second.last_outputs) {
              ctx.inputs[key] = value;
            }
          }

          ctx.on_progress = [this, task_id](float p) {
            this->handle_progress_callback(task_id, p);
          };

          stage = node.stage;
          run_events.push_back({task_id, TaskState::Running, node.task.progress});
          should_execute = true;
        }
      }

      dispatch_events(run_events);
      if (!should_execute) {
        cv_.notify_all();
        continue;
      }

      auto result = stage->execute(ctx);
      finalize_execution(task_id, ctx, result);
    }
  }

  void handle_progress_callback(const std::string &task_id, float progress) {
    std::vector<StateEvent> immediate_events;
    std::vector<StateEvent> post_wait_events;
    bool should_wait_for_resume = false;

    std::unique_lock<std::mutex> lock(mutex_);
    auto it = nodes_.find(task_id);
    if (it == nodes_.end()) {
      return;
    }

    auto &node = it->second;
    node.task.set_progress(progress);
    if (node.task.state == TaskState::Running) {
      immediate_events.push_back({task_id, TaskState::Running, node.task.progress});
    }

    if (node.pause_requested && node.task.state == TaskState::Running) {
      auto paused = node.task.transition_to(TaskState::Paused);
      if (paused.is_ok()) {
        node.pause_requested = false;
        node.pause_deadline.reset();
        immediate_events.push_back({task_id, TaskState::Paused, node.task.progress});
        should_wait_for_resume = true;
        cv_.notify_all();
      }
    }

    lock.unlock();
    dispatch_events(immediate_events);
    if (!should_wait_for_resume) {
      return;
    }

    lock.lock();
    while (!stopping_) {
      auto current = nodes_.find(task_id);
      if (current == nodes_.end()) {
        break;
      }
      if (current->second.task.state != TaskState::Paused) {
        break;
      }
      cv_.wait(lock);
    }

    auto current = nodes_.find(task_id);
    if (current != nodes_.end() && current->second.task.state == TaskState::Running) {
      post_wait_events.push_back(
          {task_id, TaskState::Running, current->second.task.progress});
    }
    lock.unlock();

    dispatch_events(post_wait_events);
  }

  void finalize_execution(const std::string &task_id, StageContext &ctx,
                          const Result<void, TaskError> &result) {
    std::vector<StateEvent> events;

    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = nodes_.find(task_id);
      if (it == nodes_.end()) {
        return;
      }

      Node &node = it->second;
      if (node.running) {
        node.running = false;
        running_set_.erase(task_id);
        release_resources_locked(node.task.resource_demand);
      }

      if (node.task.state == TaskState::Canceled) {
        propagate_dependency_canceled_locked(task_id, events);
        cv_.notify_all();
        // Do not overwrite canceled state, even if stage returned success.
      } else if (result.is_ok()) {
        auto succeeded = node.task.transition_to(TaskState::Succeeded);
        if (succeeded.is_ok()) {
          node.task.set_progress(1.0F);
          node.last_outputs = ctx.outputs;
          events.push_back({task_id, TaskState::Succeeded, 1.0F});
          wake_successors_locked(task_id, events);
        } else {
          node.task.error = succeeded.error();
          auto failed = node.task.transition_to(TaskState::Failed);
          if (failed.is_ok()) {
            events.push_back({task_id, TaskState::Failed, node.task.progress});
          }
          propagate_dependency_canceled_locked(task_id, events);
        }
      } else {
        const auto &err = result.error();
        node.task.error = err;
        const bool canceled = err.category == ErrorCategory::Canceled ||
                              (node.task.cancel_token &&
                               node.task.cancel_token->is_canceled());

        if (canceled) {
          auto to_canceled = node.task.transition_to(TaskState::Canceled);
          if (to_canceled.is_ok()) {
            events.push_back({task_id, TaskState::Canceled, node.task.progress});
          }
        } else {
          auto to_failed = node.task.transition_to(TaskState::Failed);
          if (to_failed.is_ok()) {
            events.push_back({task_id, TaskState::Failed, node.task.progress});
          }
        }

        propagate_dependency_canceled_locked(task_id, events);
      }
    }

    dispatch_events(events);
    cv_.notify_all();
  }

  void wake_successors_locked(const std::string &task_id,
                              std::vector<StateEvent> &events) {
    auto succ_it = successors_.find(task_id);
    if (succ_it == successors_.end()) {
      return;
    }

    for (const auto &succ_id : succ_it->second) {
      auto node_it = nodes_.find(succ_id);
      if (node_it == nodes_.end()) {
        continue;
      }
      auto &succ = node_it->second;
      if (succ.task.state != TaskState::Queued || succ.unmet_deps == 0) {
        continue;
      }

      succ.unmet_deps--;
      if (succ.unmet_deps == 0) {
        auto ready = succ.task.transition_to(TaskState::Ready);
        if (ready.is_ok()) {
          succ.ready_since = Clock::now();
          ready_set_.insert(succ_id);
          events.push_back({succ_id, TaskState::Ready, succ.task.progress});
        }
      }
    }
  }

  void propagate_dependency_canceled_locked(const std::string &root_id,
                                            std::vector<StateEvent> &events) {
    std::vector<std::string> stack;
    std::unordered_set<std::string> visited;
    stack.push_back(root_id);

    while (!stack.empty()) {
      const auto current = stack.back();
      stack.pop_back();

      auto succ_it = successors_.find(current);
      if (succ_it == successors_.end()) {
        continue;
      }

      for (const auto &succ_id : succ_it->second) {
        if (!visited.insert(succ_id).second) {
          continue;
        }

        auto node_it = nodes_.find(succ_id);
        if (node_it == nodes_.end()) {
          continue;
        }
        auto &node = node_it->second;

        if (is_terminal(node.task.state)) {
          stack.push_back(succ_id);
          continue;
        }

        if (node.task.cancel_token) {
          node.task.cancel_token->request_cancel();
        }
        if (node.task.state == TaskState::Ready) {
          ready_set_.erase(succ_id);
        }

        node.task.error = TaskError(
            ErrorCategory::Canceled, 3004, false,
            "Task canceled due to dependency failure",
            "Ancestor task failed or canceled", {
                {"dependency_task_id", current},
            });

        auto to_canceled = node.task.transition_to(TaskState::Canceled);
        if (to_canceled.is_ok()) {
          events.push_back({succ_id, TaskState::Canceled, node.task.progress});
        }

        stack.push_back(succ_id);
      }
    }
  }

  [[nodiscard]] bool has_runnable_task_locked() const {
    return pick_candidate_locked(/*allow_escape=*/true).has_value();
  }

  [[nodiscard]] bool fits_cpu_hard_locked(const ResourceDemand &demand) const {
    return resource_in_use_.cpu_slots + demand.cpu_slots <=
           config_.resource_budget.cpu_slots_hard;
  }

  [[nodiscard]] bool fits_soft_locked(const ResourceDemand &demand) const {
    const bool ram_ok = config_.resource_budget.ram_soft_mb <= 0 ||
                        resource_in_use_.ram_mb + demand.ram_mb <=
                            config_.resource_budget.ram_soft_mb;
    const bool vram_ok = config_.resource_budget.vram_soft_mb <= 0 ||
                         resource_in_use_.vram_mb + demand.vram_mb <=
                             config_.resource_budget.vram_soft_mb;
    return ram_ok && vram_ok;
  }

  [[nodiscard]] std::optional<Candidate>
  pick_candidate_locked(bool allow_escape) const {
    const auto now = Clock::now();
    std::optional<Candidate> best_soft_fit;
    std::optional<Candidate> best_soft_over;

    for (const auto &task_id : ready_set_) {
      auto it = nodes_.find(task_id);
      if (it == nodes_.end()) {
        continue;
      }
      const auto &node = it->second;
      if (node.task.state != TaskState::Ready) {
        continue;
      }
      if (!fits_cpu_hard_locked(node.task.resource_demand)) {
        continue;
      }

      const auto wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               now - node.ready_since)
                               .count();
      const auto intervals =
          wait_ms / std::max(1, config_.aging_policy.interval_ms);
      const long long score =
          static_cast<long long>(node.task.priority) +
          static_cast<long long>(intervals) *
              static_cast<long long>(config_.aging_policy.boost_per_interval);

      Candidate candidate;
      candidate.task_id = task_id;
      candidate.effective_priority = score;
      candidate.ready_since = node.ready_since;
      candidate.soft_fit = fits_soft_locked(node.task.resource_demand);

      auto better_than = [](const Candidate &lhs, const Candidate &rhs) {
        if (lhs.effective_priority != rhs.effective_priority) {
          return lhs.effective_priority > rhs.effective_priority;
        }
        if (lhs.ready_since != rhs.ready_since) {
          return lhs.ready_since < rhs.ready_since;
        }
        return lhs.task_id < rhs.task_id;
      };

      if (candidate.soft_fit) {
        if (!best_soft_fit.has_value() || better_than(candidate, *best_soft_fit)) {
          best_soft_fit = candidate;
        }
      } else if (!best_soft_over.has_value() ||
                 better_than(candidate, *best_soft_over)) {
        best_soft_over = candidate;
      }
    }

    if (best_soft_fit.has_value()) {
      return best_soft_fit;
    }

    if (allow_escape && running_set_.empty() && best_soft_over.has_value()) {
      return best_soft_over;
    }

    return std::nullopt;
  }

  void reserve_resources_locked(const ResourceDemand &demand) {
    resource_in_use_.cpu_slots += demand.cpu_slots;
    resource_in_use_.ram_mb += demand.ram_mb;
    resource_in_use_.vram_mb += demand.vram_mb;
  }

  void release_resources_locked(const ResourceDemand &demand) {
    resource_in_use_.cpu_slots = std::max(0, resource_in_use_.cpu_slots - demand.cpu_slots);
    resource_in_use_.ram_mb = std::max(0, resource_in_use_.ram_mb - demand.ram_mb);
    resource_in_use_.vram_mb =
        std::max(0, resource_in_use_.vram_mb - demand.vram_mb);
  }

  [[nodiscard]] bool creates_cycle_locked(
      const std::string &task_id, const std::vector<std::string> &deps) const {
    // Strict dependency submission order prevents cycles in normal flow,
    // but keep a defensive DFS guard in case edges are built dynamically.
    std::vector<std::string> stack;
    std::unordered_set<std::string> visited;

    for (const auto &dep : deps) {
      stack.push_back(dep);
    }

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();
      if (current == task_id) {
        return true;
      }
      if (!visited.insert(current).second) {
        continue;
      }

      auto it = successors_.find(current);
      if (it == successors_.end()) {
        continue;
      }
      for (const auto &next : it->second) {
        stack.push_back(next);
      }
    }

    return false;
  }

  void dispatch_events(const std::vector<StateEvent> &events) {
    if (events.empty()) {
      return;
    }

    std::vector<StateCallback> callbacks;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      callbacks = callbacks_;
    }

    for (const auto &event : events) {
      for (const auto &cb : callbacks) {
        if (cb) {
          cb(event.task_id, event.state, event.progress);
        }
      }
    }
  }

  SchedulerConfig config_;
  std::shared_ptr<ILogger> logger_;

  mutable std::mutex mutex_;
  std::condition_variable cv_;
  bool stopping_ = false;

  std::unordered_map<std::string, Node> nodes_;
  std::unordered_map<std::string, std::vector<std::string>> successors_;
  std::unordered_set<std::string> ready_set_;
  std::unordered_set<std::string> running_set_;
  ResourceUsage resource_in_use_{};

  std::vector<std::thread> workers_;
  std::vector<StateCallback> callbacks_;
};

} // namespace

std::unique_ptr<IScheduler>
create_thread_pool_scheduler(const SchedulerConfig &config,
                             std::shared_ptr<ILogger> logger) {
  return std::make_unique<ThreadPoolScheduler>(config, std::move(logger));
}

} // namespace stv::core
