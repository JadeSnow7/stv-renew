#include <gtest/gtest.h>

#include "core/cancel_token.h"
#include "core/task.h"

using namespace stv::core;

// ============================================================
// Test: Legal state transitions
// ============================================================

TEST(TaskStateMachine, QueuedToReady) {
  TaskDescriptor t;
  t.task_id = "t-001";
  t.type = TaskType::Storyboard;
  ASSERT_EQ(t.state, TaskState::Queued);

  auto result = t.transition_to(TaskState::Ready);
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(t.state, TaskState::Ready);
}

TEST(TaskStateMachine, QueuedToPaused) {
  TaskDescriptor t;
  t.task_id = "t-001a";
  t.type = TaskType::Storyboard;

  auto result = t.transition_to(TaskState::Paused);
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(t.state, TaskState::Paused);
  ASSERT_TRUE(t.paused_from.has_value());
  ASSERT_EQ(*t.paused_from, TaskState::Queued);
}

TEST(TaskStateMachine, ReadyToRunning) {
  TaskDescriptor t;
  t.task_id = "t-002";
  t.type = TaskType::ImageGen;
  t.transition_to(TaskState::Ready);

  auto result = t.transition_to(TaskState::Running);
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(t.state, TaskState::Running);
  ASSERT_TRUE(t.started_at.has_value());
}

TEST(TaskStateMachine, ReadyToPaused) {
  TaskDescriptor t;
  t.task_id = "t-002a";
  t.type = TaskType::ImageGen;
  t.transition_to(TaskState::Ready);

  auto result = t.transition_to(TaskState::Paused);
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(t.state, TaskState::Paused);
  ASSERT_TRUE(t.paused_from.has_value());
  ASSERT_EQ(*t.paused_from, TaskState::Ready);
}

TEST(TaskStateMachine, RunningToSucceeded) {
  TaskDescriptor t;
  t.task_id = "t-003";
  t.type = TaskType::Compose;
  t.transition_to(TaskState::Ready);
  t.transition_to(TaskState::Running);

  auto result = t.transition_to(TaskState::Succeeded);
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(t.state, TaskState::Succeeded);
  ASSERT_TRUE(t.finished_at.has_value());
  ASSERT_TRUE(is_terminal(t.state));
}

TEST(TaskStateMachine, RunningToFailed) {
  TaskDescriptor t;
  t.task_id = "t-004";
  t.type = TaskType::TTS;
  t.transition_to(TaskState::Ready);
  t.transition_to(TaskState::Running);

  auto result = t.transition_to(TaskState::Failed);
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(t.state, TaskState::Failed);
  ASSERT_TRUE(is_terminal(t.state));
}

TEST(TaskStateMachine, RunningToPaused) {
  TaskDescriptor t;
  t.task_id = "t-005";
  t.type = TaskType::Storyboard;
  t.transition_to(TaskState::Ready);
  t.transition_to(TaskState::Running);

  auto result = t.transition_to(TaskState::Paused);
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(t.state, TaskState::Paused);
}

TEST(TaskStateMachine, PausedToRunning) {
  TaskDescriptor t;
  t.task_id = "t-006";
  t.type = TaskType::ImageGen;
  t.transition_to(TaskState::Ready);
  t.transition_to(TaskState::Running);
  t.transition_to(TaskState::Paused);

  auto result = t.transition_to(TaskState::Running);
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(t.state, TaskState::Running);
  ASSERT_FALSE(t.paused_from.has_value());
}

TEST(TaskStateMachine, PausedToQueued) {
  TaskDescriptor t;
  t.task_id = "t-006a";
  t.type = TaskType::ImageGen;
  t.transition_to(TaskState::Paused);

  auto result = t.transition_to(TaskState::Queued);
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(t.state, TaskState::Queued);
  ASSERT_FALSE(t.paused_from.has_value());
}

TEST(TaskStateMachine, PausedToReady) {
  TaskDescriptor t;
  t.task_id = "t-006b";
  t.type = TaskType::ImageGen;
  t.transition_to(TaskState::Ready);
  t.transition_to(TaskState::Paused);

  auto result = t.transition_to(TaskState::Ready);
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(t.state, TaskState::Ready);
  ASSERT_FALSE(t.paused_from.has_value());
}

TEST(TaskStateMachine, RunningToCanceled) {
  TaskDescriptor t;
  t.task_id = "t-007";
  t.type = TaskType::Compose;
  t.transition_to(TaskState::Ready);
  t.transition_to(TaskState::Running);

  auto result = t.transition_to(TaskState::Canceled);
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(t.state, TaskState::Canceled);
  ASSERT_TRUE(is_terminal(t.state));
}

TEST(TaskStateMachine, QueuedToCanceled) {
  TaskDescriptor t;
  t.task_id = "t-008";
  t.type = TaskType::ImageGen;

  auto result = t.transition_to(TaskState::Canceled);
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(t.state, TaskState::Canceled);
}

// ---- Cancel from all non-terminal states ----

TEST(TaskStateMachine, ReadyToCanceled) {
  TaskDescriptor t;
  t.task_id = "t-009";
  t.type = TaskType::ImageGen;
  t.transition_to(TaskState::Ready);

  auto result = t.transition_to(TaskState::Canceled);
  ASSERT_TRUE(result.is_ok());
}

TEST(TaskStateMachine, PausedToCanceled) {
  TaskDescriptor t;
  t.task_id = "t-010";
  t.type = TaskType::Compose;
  t.transition_to(TaskState::Ready);
  t.transition_to(TaskState::Running);
  t.transition_to(TaskState::Paused);

  auto result = t.transition_to(TaskState::Canceled);
  ASSERT_TRUE(result.is_ok());
}

// ---- Retry: Failed → Queued ----

TEST(TaskStateMachine, FailedToQueuedRetry) {
  TaskDescriptor t;
  t.task_id = "t-011";
  t.type = TaskType::Storyboard;
  t.transition_to(TaskState::Ready);
  t.transition_to(TaskState::Running);
  t.set_progress(0.5f);
  t.error = TaskError::Pipeline("Test error");
  t.transition_to(TaskState::Failed);

  auto result = t.transition_to(TaskState::Queued);
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(t.state, TaskState::Queued);
  ASSERT_FLOAT_EQ(t.progress, 0.0f);      // Progress reset
  ASSERT_FALSE(t.error.has_value());      // Error cleared
  ASSERT_FALSE(t.started_at.has_value()); // Timestamps reset
}

// ============================================================
// Test: Illegal state transitions
// ============================================================

TEST(TaskStateMachine, IllegalQueuedToRunning) {
  TaskDescriptor t;
  t.task_id = "t-012";
  t.type = TaskType::ImageGen;

  auto result = t.transition_to(TaskState::Running);
  ASSERT_TRUE(result.is_err());
  ASSERT_EQ(t.state, TaskState::Queued); // State unchanged
}

TEST(TaskStateMachine, IllegalSucceededToRunning) {
  TaskDescriptor t;
  t.task_id = "t-013";
  t.type = TaskType::Compose;
  t.transition_to(TaskState::Ready);
  t.transition_to(TaskState::Running);
  t.transition_to(TaskState::Succeeded);

  auto result = t.transition_to(TaskState::Running);
  ASSERT_TRUE(result.is_err());
  ASSERT_EQ(t.state, TaskState::Succeeded);
}

TEST(TaskStateMachine, IllegalCanceledToRunning) {
  TaskDescriptor t;
  t.task_id = "t-014";
  t.type = TaskType::TTS;
  t.transition_to(TaskState::Canceled);

  auto result = t.transition_to(TaskState::Running);
  ASSERT_TRUE(result.is_err());
}

TEST(TaskStateMachine, IllegalQueuedToSucceeded) {
  TaskDescriptor t;
  t.task_id = "t-015";
  t.type = TaskType::ImageGen;

  auto result = t.transition_to(TaskState::Succeeded);
  ASSERT_TRUE(result.is_err());
}

TEST(TaskStateMachine, IllegalQueuedToFailed) {
  TaskDescriptor t;
  t.task_id = "t-016";
  t.type = TaskType::Compose;

  auto result = t.transition_to(TaskState::Failed);
  ASSERT_TRUE(result.is_err());
}

TEST(TaskStateMachine, IllegalPausedToSucceeded) {
  TaskDescriptor t;
  t.task_id = "t-017";
  t.type = TaskType::Storyboard;
  t.transition_to(TaskState::Ready);
  t.transition_to(TaskState::Running);
  t.transition_to(TaskState::Paused);

  auto result = t.transition_to(TaskState::Succeeded);
  ASSERT_TRUE(result.is_err());
}

// ============================================================
// Test: Progress and utility
// ============================================================

TEST(TaskStateMachine, ProgressClamp) {
  TaskDescriptor t;
  t.set_progress(-0.5f);
  ASSERT_FLOAT_EQ(t.progress, 0.0f);

  t.set_progress(1.5f);
  ASSERT_FLOAT_EQ(t.progress, 1.0f);

  t.set_progress(0.42f);
  ASSERT_FLOAT_EQ(t.progress, 0.42f);
}

TEST(TaskStateMachine, ToStringCoverage) {
  ASSERT_STREQ(to_string(TaskState::Queued), "Queued");
  ASSERT_STREQ(to_string(TaskState::Running), "Running");
  ASSERT_STREQ(to_string(TaskState::Succeeded), "Succeeded");
  ASSERT_STREQ(to_string(TaskType::Storyboard), "Storyboard");
  ASSERT_STREQ(to_string(TaskType::Compose), "Compose");
}

// ============================================================
// Test: CancelToken
// ============================================================

TEST(CancelToken, BasicCancelFlow) {
  auto token = CancelToken::create();
  ASSERT_FALSE(token->is_canceled());

  token->request_cancel();
  ASSERT_TRUE(token->is_canceled());
}

TEST(CancelToken, IdempotentCancel) {
  auto token = CancelToken::create();
  int callback_count = 0;
  token->on_cancel([&]() { callback_count++; });

  token->request_cancel();
  token->request_cancel(); // Should not invoke callback again
  ASSERT_EQ(callback_count, 1);
}

TEST(CancelToken, CallbackInvokedOnCancel) {
  auto token = CancelToken::create();
  bool called = false;
  token->on_cancel([&]() { called = true; });

  ASSERT_FALSE(called);
  token->request_cancel();
  ASSERT_TRUE(called);
}

TEST(CancelToken, CallbackInvokedImmediatelyIfAlreadyCanceled) {
  auto token = CancelToken::create();
  token->request_cancel();

  bool called = false;
  token->on_cancel([&]() { called = true; });
  ASSERT_TRUE(called); // Invoked immediately
}

TEST(CancelToken, ThrowIfCanceled) {
  auto token = CancelToken::create();
  ASSERT_NO_THROW(token->throw_if_canceled());

  token->request_cancel();
  ASSERT_THROW(token->throw_if_canceled(), std::runtime_error);
}
