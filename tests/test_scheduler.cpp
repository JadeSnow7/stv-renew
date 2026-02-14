#include <gtest/gtest.h>

#include "core/cancel_token.h"
#include "core/pipeline.h"
#include "core/scheduler.h"
#include "core/task.h"

using namespace stv::core;

// Forward declaration — defined in scheduler.cpp
namespace stv::core {
std::unique_ptr<IScheduler> create_simple_scheduler();
}

// ============ Helper: Counting Stage ============

class CountingStage : public IStage {
public:
  std::string name() const override { return "CountingStage"; }

  Result<void, TaskError> execute(StageContext &ctx) override {
    execution_count++;
    if (ctx.on_progress)
      ctx.on_progress(1.0f);
    ctx.set_output("result", std::string("done"));
    return Result<void, TaskError>::Ok();
  }

  int execution_count = 0;
};

class FailingStage : public IStage {
public:
  std::string name() const override { return "FailingStage"; }

  Result<void, TaskError> execute(StageContext & /*ctx*/) override {
    return Result<void, TaskError>::Err(
        TaskError::Pipeline("Intentional failure"));
  }
};

class CancelCheckStage : public IStage {
public:
  std::string name() const override { return "CancelCheckStage"; }

  Result<void, TaskError> execute(StageContext &ctx) override {
    if (ctx.cancel_token && ctx.cancel_token->is_canceled()) {
      return Result<void, TaskError>::Err(TaskError::Canceled());
    }
    return Result<void, TaskError>::Ok();
  }
};

// ============================================================
// Tests
// ============================================================

TEST(Scheduler, SubmitAndExecute) {
  auto scheduler = create_simple_scheduler();
  auto stage = std::make_shared<CountingStage>();

  TaskDescriptor task;
  task.task_id = "s-001";
  task.trace_id = "trace-1";
  task.type = TaskType::Storyboard;
  // No deps → auto-promoted to Ready

  std::vector<std::pair<std::string, TaskState>> events;
  scheduler->on_state_change([&](const std::string &id, TaskState s, float) {
    events.emplace_back(id, s);
  });

  scheduler->submit(std::move(task), stage);
  scheduler->tick();

  ASSERT_EQ(stage->execution_count, 1);
  // Events: Ready (auto-promote), Running, Succeeded
  // Note: the initial Ready promotion happens in submit()
  ASSERT_GE(events.size(), 2u); // At least Running + Succeeded
  ASSERT_EQ(events.back().second, TaskState::Succeeded);
}

TEST(Scheduler, DependencyChain) {
  auto scheduler = create_simple_scheduler();
  auto stage1 = std::make_shared<CountingStage>();
  auto stage2 = std::make_shared<CountingStage>();

  TaskDescriptor task1;
  task1.task_id = "dep-001";
  task1.trace_id = "trace-2";
  task1.type = TaskType::Storyboard;
  // No deps

  TaskDescriptor task2;
  task2.task_id = "dep-002";
  task2.trace_id = "trace-2";
  task2.type = TaskType::ImageGen;
  task2.deps = {"dep-001"}; // Depends on task1

  scheduler->submit(std::move(task1), stage1);
  scheduler->submit(std::move(task2), stage2);

  // First tick: should execute task1
  scheduler->tick();
  ASSERT_EQ(stage1->execution_count, 1);
  ASSERT_EQ(stage2->execution_count, 0);

  // Second tick: task2's dep is now Succeeded → should execute
  scheduler->tick();
  ASSERT_EQ(stage2->execution_count, 1);

  ASSERT_FALSE(scheduler->has_pending_tasks());
}

TEST(Scheduler, PriorityOrdering) {
  auto scheduler = create_simple_scheduler();
  auto low_stage = std::make_shared<CountingStage>();
  auto high_stage = std::make_shared<CountingStage>();

  TaskDescriptor low;
  low.task_id = "pri-low";
  low.trace_id = "trace-3";
  low.type = TaskType::ImageGen;
  low.priority = 1;

  TaskDescriptor high;
  high.task_id = "pri-high";
  high.trace_id = "trace-3";
  high.type = TaskType::Storyboard;
  high.priority = 100;

  scheduler->submit(std::move(low), low_stage);
  scheduler->submit(std::move(high), high_stage);

  // First tick: should pick high priority
  scheduler->tick();
  ASSERT_EQ(high_stage->execution_count, 1);
  ASSERT_EQ(low_stage->execution_count, 0);

  // Second tick: now low
  scheduler->tick();
  ASSERT_EQ(low_stage->execution_count, 1);
}

TEST(Scheduler, FailedTask) {
  auto scheduler = create_simple_scheduler();
  auto stage = std::make_shared<FailingStage>();

  TaskDescriptor task;
  task.task_id = "fail-001";
  task.trace_id = "trace-4";
  task.type = TaskType::Compose;

  std::vector<TaskState> states;
  scheduler->on_state_change(
      [&](const std::string &, TaskState s, float) { states.push_back(s); });

  scheduler->submit(std::move(task), stage);
  scheduler->tick();

  ASSERT_FALSE(states.empty());
  ASSERT_EQ(states.back(), TaskState::Failed);
}

TEST(Scheduler, CancelBeforeExecution) {
  auto scheduler = create_simple_scheduler();
  auto token = CancelToken::create();
  auto stage = std::make_shared<CancelCheckStage>();

  TaskDescriptor task;
  task.task_id = "cancel-001";
  task.trace_id = "trace-5";
  task.type = TaskType::ImageGen;
  task.cancel_token = token;

  scheduler->submit(std::move(task), stage);

  // Cancel before tick
  auto result = scheduler->cancel("cancel-001");
  ASSERT_TRUE(result.is_ok());

  ASSERT_FALSE(scheduler->has_pending_tasks());
}

TEST(Scheduler, NoPendingAfterCompletion) {
  auto scheduler = create_simple_scheduler();
  auto stage = std::make_shared<CountingStage>();

  TaskDescriptor task;
  task.task_id = "np-001";
  task.trace_id = "trace-6";
  task.type = TaskType::TTS;

  scheduler->submit(std::move(task), stage);
  ASSERT_TRUE(scheduler->has_pending_tasks());

  scheduler->tick();
  ASSERT_FALSE(scheduler->has_pending_tasks());
}
