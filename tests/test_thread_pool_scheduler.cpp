#include <gtest/gtest.h>

#include "core/pipeline.h"
#include "core/scheduler.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace stv::core;

namespace {

using Clock = std::chrono::steady_clock;

class FixedWorkStage : public IStage {
public:
  FixedWorkStage(int steps, int step_ms, bool emit_progress = true,
                 bool check_cancel = true,
                 std::atomic<int> *running_counter = nullptr,
                 std::atomic<int> *max_running = nullptr,
                 std::atomic<int> *execution_count = nullptr)
      : steps_(steps), step_ms_(step_ms), emit_progress_(emit_progress),
        check_cancel_(check_cancel), running_counter_(running_counter),
        max_running_(max_running), execution_count_(execution_count) {}

  std::string name() const override { return "FixedWorkStage"; }

  Result<void, TaskError> execute(StageContext &ctx) override {
    if (execution_count_) {
      execution_count_->fetch_add(1);
    }

    RunningGuard guard(running_counter_, max_running_);
    for (int i = 0; i < steps_; ++i) {
      if (check_cancel_ && ctx.cancel_token && ctx.cancel_token->is_canceled()) {
        return Result<void, TaskError>::Err(TaskError::Canceled());
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(step_ms_));
      if (emit_progress_ && ctx.on_progress) {
        const float progress = static_cast<float>(i + 1) /
                               static_cast<float>(std::max(steps_, 1));
        ctx.on_progress(progress);
      }
    }

    return Result<void, TaskError>::Ok();
  }

private:
  class RunningGuard {
  public:
    RunningGuard(std::atomic<int> *running_counter, std::atomic<int> *max_running)
        : running_counter_(running_counter), max_running_(max_running) {
      if (!running_counter_) {
        return;
      }
      const int now = running_counter_->fetch_add(1) + 1;
      if (!max_running_) {
        return;
      }
      int observed = max_running_->load();
      while (observed < now &&
             !max_running_->compare_exchange_weak(observed, now)) {
      }
    }

    ~RunningGuard() {
      if (running_counter_) {
        running_counter_->fetch_sub(1);
      }
    }

  private:
    std::atomic<int> *running_counter_;
    std::atomic<int> *max_running_;
  };

  int steps_;
  int step_ms_;
  bool emit_progress_;
  bool check_cancel_;
  std::atomic<int> *running_counter_;
  std::atomic<int> *max_running_;
  std::atomic<int> *execution_count_;
};

class LambdaStage : public IStage {
public:
  explicit LambdaStage(std::function<Result<void, TaskError>(StageContext &)> fn)
      : fn_(std::move(fn)) {}

  std::string name() const override { return "LambdaStage"; }

  Result<void, TaskError> execute(StageContext &ctx) override { return fn_(ctx); }

private:
  std::function<Result<void, TaskError>(StageContext &)> fn_;
};

struct EventLog {
  std::mutex mutex;
  std::condition_variable cv;
  std::vector<std::pair<std::string, TaskState>> events;

  void push(const std::string &task_id, TaskState state) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      events.emplace_back(task_id, state);
    }
    cv.notify_all();
  }

  bool has_event(const std::string &task_id, TaskState state) {
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto &[id, s] : events) {
      if (id == task_id && s == state) {
        return true;
      }
    }
    return false;
  }

  int first_index(const std::string &task_id, TaskState state) {
    std::lock_guard<std::mutex> lock(mutex);
    for (size_t i = 0; i < events.size(); ++i) {
      if (events[i].first == task_id && events[i].second == state) {
        return static_cast<int>(i);
      }
    }
    return -1;
  }
};

SchedulerConfig make_config() {
  SchedulerConfig cfg;
  cfg.worker_count = 2;
  cfg.resource_budget.cpu_slots_hard = 2;
  cfg.resource_budget.ram_soft_mb = 2048;
  cfg.resource_budget.vram_soft_mb = 7680;
  cfg.aging_policy.interval_ms = 100;
  cfg.aging_policy.boost_per_interval = 1;
  cfg.pause_policy.checkpoint_timeout_ms = 500;
  return cfg;
}

TaskDescriptor make_task(std::string id, int priority = 0) {
  TaskDescriptor t;
  t.task_id = std::move(id);
  t.trace_id = "trace";
  t.type = TaskType::ImageGen;
  t.priority = priority;
  t.cancel_token = CancelToken::create();
  return t;
}

bool wait_until_idle(IScheduler *scheduler, std::chrono::milliseconds timeout) {
  const auto deadline = Clock::now() + timeout;
  while (Clock::now() < deadline) {
    scheduler->tick();
    if (!scheduler->has_pending_tasks()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return !scheduler->has_pending_tasks();
}

} // namespace

TEST(ThreadPoolScheduler, RejectsMissingDependency) {
  auto scheduler = create_thread_pool_scheduler(make_config(), nullptr);

  auto task = make_task("t-missing");
  task.deps = {"unknown"};

  auto result = scheduler->submit(
      std::move(task), std::make_shared<FixedWorkStage>(1, 1));
  ASSERT_TRUE(result.is_err());
}

TEST(ThreadPoolScheduler, RejectsDuplicateTaskId) {
  auto scheduler = create_thread_pool_scheduler(make_config(), nullptr);

  auto first = make_task("dup");
  auto second = make_task("dup");

  ASSERT_TRUE(scheduler
                  ->submit(std::move(first), std::make_shared<FixedWorkStage>(1, 5))
                  .is_ok());
  auto duplicate =
      scheduler->submit(std::move(second), std::make_shared<FixedWorkStage>(1, 5));
  ASSERT_TRUE(duplicate.is_err());
  ASSERT_TRUE(wait_until_idle(scheduler.get(), std::chrono::seconds(2)));
}

TEST(ThreadPoolScheduler, DagWakeupOnlySuccessors) {
  auto scheduler = create_thread_pool_scheduler(make_config(), nullptr);
  EventLog log;
  scheduler->on_state_change([&](const std::string &task_id, TaskState state, float) {
    log.push(task_id, state);
  });

  auto a = make_task("a", 100);
  auto b = make_task("b", 50);
  auto c = make_task("c", 40);
  auto d = make_task("d", 50);
  b.deps = {"a"};
  c.deps = {"b"};
  d.deps = {"a"};

  ASSERT_TRUE(scheduler->submit(std::move(a), std::make_shared<FixedWorkStage>(1, 15)).is_ok());
  ASSERT_TRUE(scheduler->submit(std::move(b), std::make_shared<FixedWorkStage>(4, 20)).is_ok());
  ASSERT_TRUE(scheduler->submit(std::move(c), std::make_shared<FixedWorkStage>(1, 10)).is_ok());
  ASSERT_TRUE(scheduler->submit(std::move(d), std::make_shared<FixedWorkStage>(2, 20)).is_ok());

  ASSERT_TRUE(wait_until_idle(scheduler.get(), std::chrono::seconds(4)));

  const int succ_a = log.first_index("a", TaskState::Succeeded);
  const int ready_b = log.first_index("b", TaskState::Ready);
  const int ready_d = log.first_index("d", TaskState::Ready);
  const int succ_b = log.first_index("b", TaskState::Succeeded);
  const int ready_c = log.first_index("c", TaskState::Ready);

  ASSERT_GE(succ_a, 0);
  ASSERT_GT(ready_b, succ_a);
  ASSERT_GT(ready_d, succ_a);
  ASSERT_GT(ready_c, succ_b);
}

TEST(ThreadPoolScheduler, AgingPreventsStarvation) {
  auto cfg = make_config();
  cfg.worker_count = 1;
  cfg.resource_budget.cpu_slots_hard = 1;
  cfg.aging_policy.interval_ms = 10;
  cfg.aging_policy.boost_per_interval = 50;
  auto scheduler = create_thread_pool_scheduler(cfg, nullptr);

  std::atomic<bool> producer_done{false};
  std::atomic<bool> producer_submit_failed{false};
  std::atomic<bool> low_done{false};
  std::atomic<bool> low_before_producer_done{false};

  auto high_stage = std::make_shared<FixedWorkStage>(1, 12);
  auto low_stage = std::make_shared<LambdaStage>(
      [&](StageContext &) {
        low_before_producer_done.store(!producer_done.load());
        low_done.store(true);
        return Result<void, TaskError>::Ok();
      });

  auto first_high = make_task("h0", 100);
  ASSERT_TRUE(scheduler->submit(std::move(first_high), high_stage).is_ok());

  auto low = make_task("low", 0);
  ASSERT_TRUE(scheduler->submit(std::move(low), low_stage).is_ok());

  std::thread producer([&]() {
    for (int i = 1; i <= 80; ++i) {
      auto h = make_task("h" + std::to_string(i), 100);
      auto submit_result = scheduler->submit(std::move(h), high_stage);
      if (submit_result.is_err()) {
        producer_submit_failed.store(true);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    producer_done.store(true);
  });

  const auto deadline = Clock::now() + std::chrono::seconds(3);
  while (Clock::now() < deadline && !low_done.load()) {
    scheduler->tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  producer.join();
  ASSERT_TRUE(wait_until_idle(scheduler.get(), std::chrono::seconds(4)));
  ASSERT_FALSE(producer_submit_failed.load());
  ASSERT_TRUE(low_done.load());
  ASSERT_TRUE(low_before_producer_done.load());
}

TEST(ThreadPoolScheduler, CpuHardBudgetLimitsConcurrency) {
  auto cfg = make_config();
  cfg.worker_count = 4;
  cfg.resource_budget.cpu_slots_hard = 2;
  auto scheduler = create_thread_pool_scheduler(cfg, nullptr);

  std::atomic<int> running{0};
  std::atomic<int> max_running{0};

  for (int i = 0; i < 6; ++i) {
    auto t = make_task("cpu" + std::to_string(i), 20);
    t.resource_demand.cpu_slots = 1;
    ASSERT_TRUE(
        scheduler
            ->submit(std::move(t), std::make_shared<FixedWorkStage>(2, 40, true,
                                                                     true, &running,
                                                                     &max_running))
            .is_ok());
  }

  ASSERT_TRUE(wait_until_idle(scheduler.get(), std::chrono::seconds(4)));
  ASSERT_LE(max_running.load(), 2);
}

TEST(ThreadPoolScheduler, SoftBudgetQueuesAndEscapeSingleTask) {
  auto cfg = make_config();
  cfg.worker_count = 2;
  cfg.resource_budget.cpu_slots_hard = 2;
  cfg.resource_budget.ram_soft_mb = 100;
  cfg.resource_budget.vram_soft_mb = 100;
  auto scheduler = create_thread_pool_scheduler(cfg, nullptr);

  std::atomic<int> running{0};
  std::atomic<int> max_running{0};

  auto a = make_task("soft-a", 10);
  a.resource_demand.ram_mb = 80;
  a.resource_demand.vram_mb = 80;

  auto b = make_task("soft-b", 10);
  b.resource_demand.ram_mb = 80;
  b.resource_demand.vram_mb = 80;

  ASSERT_TRUE(scheduler->submit(std::move(a),
                                std::make_shared<FixedWorkStage>(2, 40, true,
                                                                 true, &running,
                                                                 &max_running))
                  .is_ok());
  ASSERT_TRUE(scheduler->submit(std::move(b),
                                std::make_shared<FixedWorkStage>(2, 40, true,
                                                                 true, &running,
                                                                 &max_running))
                  .is_ok());

  ASSERT_TRUE(wait_until_idle(scheduler.get(), std::chrono::seconds(4)));
  ASSERT_EQ(max_running.load(), 1);

  std::atomic<bool> heavy_ran{false};
  auto heavy = make_task("soft-heavy", 50);
  heavy.resource_demand.ram_mb = 150;
  heavy.resource_demand.vram_mb = 150;
  auto heavy_stage = std::make_shared<LambdaStage>(
      [&](StageContext &) {
        heavy_ran.store(true);
        return Result<void, TaskError>::Ok();
      });

  ASSERT_TRUE(scheduler->submit(std::move(heavy), heavy_stage).is_ok());
  ASSERT_TRUE(wait_until_idle(scheduler.get(), std::chrono::seconds(2)));
  ASSERT_TRUE(heavy_ran.load());
}

TEST(ThreadPoolScheduler, PauseQueuedAndReadyImmediate) {
  auto cfg = make_config();
  cfg.worker_count = 1;
  cfg.resource_budget.cpu_slots_hard = 1;
  auto scheduler = create_thread_pool_scheduler(cfg, nullptr);

  std::atomic<int> ready_exec{0};

  auto blocker = make_task("block", 100);
  auto queued = make_task("queued", 20);
  queued.deps = {"block"};
  auto ready = make_task("ready", 10);

  ASSERT_TRUE(
      scheduler
          ->submit(std::move(blocker), std::make_shared<FixedWorkStage>(6, 30))
          .is_ok());
  ASSERT_TRUE(
      scheduler
          ->submit(std::move(queued), std::make_shared<FixedWorkStage>(1, 5))
          .is_ok());
  ASSERT_TRUE(
      scheduler
          ->submit(std::move(ready), std::make_shared<FixedWorkStage>(1, 5, true,
                                                                       true, nullptr,
                                                                       nullptr,
                                                                       &ready_exec))
          .is_ok());

  ASSERT_TRUE(scheduler->pause("queued").is_ok());
  ASSERT_TRUE(scheduler->pause("ready").is_ok());
  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  ASSERT_EQ(ready_exec.load(), 0);

  ASSERT_TRUE(scheduler->resume("ready").is_ok());
  const auto ready_deadline = Clock::now() + std::chrono::seconds(2);
  while (Clock::now() < ready_deadline && ready_exec.load() < 1) {
    scheduler->tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_GE(ready_exec.load(), 1);

  ASSERT_TRUE(scheduler->cancel("queued").is_ok());
  ASSERT_TRUE(wait_until_idle(scheduler.get(), std::chrono::seconds(4)));
}

TEST(ThreadPoolScheduler, PauseRunningThenResume) {
  auto cfg = make_config();
  cfg.worker_count = 1;
  cfg.resource_budget.cpu_slots_hard = 1;
  cfg.pause_policy.checkpoint_timeout_ms = 1000;
  auto scheduler = create_thread_pool_scheduler(cfg, nullptr);

  EventLog log;
  scheduler->on_state_change([&](const std::string &task_id, TaskState state, float) {
    log.push(task_id, state);
  });

  auto task = make_task("pause-run", 100);
  ASSERT_TRUE(scheduler->submit(std::move(task), std::make_shared<FixedWorkStage>(20, 15)).is_ok());

  const auto run_deadline = Clock::now() + std::chrono::seconds(2);
  while (Clock::now() < run_deadline && !log.has_event("pause-run", TaskState::Running)) {
    scheduler->tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_TRUE(scheduler->pause("pause-run").is_ok());
  const auto pause_deadline = Clock::now() + std::chrono::seconds(2);
  while (Clock::now() < pause_deadline && !log.has_event("pause-run", TaskState::Paused)) {
    scheduler->tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_TRUE(log.has_event("pause-run", TaskState::Paused));

  ASSERT_TRUE(scheduler->resume("pause-run").is_ok());
  ASSERT_TRUE(wait_until_idle(scheduler.get(), std::chrono::seconds(4)));
  ASSERT_TRUE(log.has_event("pause-run", TaskState::Succeeded));
}

TEST(ThreadPoolScheduler, RunningPauseTimeoutAutoCancels) {
  auto cfg = make_config();
  cfg.worker_count = 1;
  cfg.resource_budget.cpu_slots_hard = 1;
  cfg.pause_policy.checkpoint_timeout_ms = 50;
  auto scheduler = create_thread_pool_scheduler(cfg, nullptr);

  EventLog log;
  scheduler->on_state_change([&](const std::string &task_id, TaskState state, float) {
    log.push(task_id, state);
  });

  auto task = make_task("timeout", 100);
  auto no_checkpoint = std::make_shared<LambdaStage>([](StageContext &) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    return Result<void, TaskError>::Ok();
  });
  ASSERT_TRUE(scheduler->submit(std::move(task), no_checkpoint).is_ok());

  const auto run_deadline = Clock::now() + std::chrono::seconds(2);
  while (Clock::now() < run_deadline && !log.has_event("timeout", TaskState::Running)) {
    scheduler->tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto pause_result = scheduler->pause("timeout");
  ASSERT_TRUE(pause_result.is_err());
  ASSERT_EQ(pause_result.error().category, ErrorCategory::Timeout);

  ASSERT_TRUE(wait_until_idle(scheduler.get(), std::chrono::seconds(4)));
  ASSERT_TRUE(log.has_event("timeout", TaskState::Canceled));
}

TEST(ThreadPoolScheduler, ConcurrentCancelPauseResumeNoDeadlock) {
  auto cfg = make_config();
  cfg.worker_count = 2;
  cfg.resource_budget.cpu_slots_hard = 2;
  cfg.pause_policy.checkpoint_timeout_ms = 400;
  auto scheduler = create_thread_pool_scheduler(cfg, nullptr);

  auto task = make_task("race", 100);
  ASSERT_TRUE(scheduler->submit(std::move(task), std::make_shared<FixedWorkStage>(120, 5)).is_ok());

  std::thread pauser([&]() {
    for (int i = 0; i < 30; ++i) {
      (void)scheduler->pause("race");
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  });

  std::thread resumer([&]() {
    for (int i = 0; i < 30; ++i) {
      (void)scheduler->resume("race");
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  });

  std::thread canceler([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    (void)scheduler->cancel("race");
    (void)scheduler->cancel("race");
  });

  pauser.join();
  resumer.join();
  canceler.join();

  ASSERT_TRUE(wait_until_idle(scheduler.get(), std::chrono::seconds(5)));
  ASSERT_FALSE(scheduler->has_pending_tasks());
}
