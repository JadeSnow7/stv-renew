#include <gtest/gtest.h>

#include "core/orchestrator.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace stv::core;

namespace {

class RecordingScheduler : public IScheduler {
public:
  explicit RecordingScheduler(int fail_submit_after = -1)
      : fail_submit_after_(fail_submit_after) {}

  Result<void, TaskError> submit(TaskDescriptor task,
                                 std::shared_ptr<IStage> stage) override {
    (void)stage;
    ++submit_calls;
    if (fail_submit_after_ >= 0 && submit_calls > fail_submit_after_) {
      return Result<void, TaskError>::Err(
          TaskError::Internal("injected submit failure"));
    }

    known_tasks.emplace(task.task_id, task.state);
    submitted_task_ids.push_back(task.task_id);
    return Result<void, TaskError>::Ok();
  }

  Result<void, TaskError> cancel(const std::string &task_id) override {
    ++cancel_calls;
    canceled_task_ids.push_back(task_id);
    auto it = known_tasks.find(task_id);
    if (it != known_tasks.end()) {
      it->second = TaskState::Canceled;
    }
    return Result<void, TaskError>::Ok();
  }

  Result<void, TaskError> pause(const std::string &task_id) override {
    (void)task_id;
    return Result<void, TaskError>::Ok();
  }

  Result<void, TaskError> resume(const std::string &task_id) override {
    (void)task_id;
    return Result<void, TaskError>::Ok();
  }

  void on_state_change(StateCallback cb) override { callbacks.push_back(std::move(cb)); }

  void tick() override {}

  [[nodiscard]] bool has_pending_tasks() const override { return false; }

  int submit_calls = 0;
  int cancel_calls = 0;
  std::vector<std::string> submitted_task_ids;
  std::vector<std::string> canceled_task_ids;

private:
  int fail_submit_after_ = -1;
  std::unordered_map<std::string, TaskState> known_tasks;
  std::vector<StateCallback> callbacks;
};

} // namespace

TEST(WorkflowEngine, StartWorkflowReturnsErrWhenSubmitFailsAndRollsBack) {
  auto scheduler = std::make_shared<RecordingScheduler>(1);
  WorkflowEngine engine(scheduler, nullptr);

  auto start = engine.start_workflow("story", "style", 3);
  ASSERT_TRUE(start.is_err());
  ASSERT_GE(scheduler->submit_calls, 2);
  ASSERT_GE(scheduler->cancel_calls, 1);
  ASSERT_FALSE(scheduler->submitted_task_ids.empty());
}

TEST(WorkflowEngine, StartWorkflowReturnsTraceOnSuccess) {
  auto scheduler = std::make_shared<RecordingScheduler>();
  WorkflowEngine engine(scheduler, nullptr);

  auto start = engine.start_workflow("story", "style", 2);
  ASSERT_TRUE(start.is_ok());
  ASSERT_FALSE(start.value().empty());
  ASSERT_EQ(scheduler->cancel_calls, 0);
  ASSERT_EQ(scheduler->submit_calls, 4); // 1 storyboard + 2 image + 1 compose
}
