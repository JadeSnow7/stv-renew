#include <gtest/gtest.h>

#include "core/cancel_token.h"
#include "core/pipeline.h"
#include "core/task.h"

using namespace stv::core;

// Forward declaration — defined in pipeline.cpp
namespace stv::core {
std::shared_ptr<IStage> create_mock_stage(TaskType type);
}

// ============================================================
// Tests
// ============================================================

TEST(Pipeline, MockStoryboardExecutes) {
  auto stage = create_mock_stage(TaskType::Storyboard);
  ASSERT_EQ(stage->name(), "MockStoryboard");

  StageContext ctx;
  ctx.trace_id = "test-trace";
  ctx.cancel_token = CancelToken::create();
  ctx.inputs["scene_count"] = 3;

  float last_progress = 0.0f;
  ctx.on_progress = [&](float p) { last_progress = p; };

  auto result = stage->execute(ctx);
  ASSERT_TRUE(result.is_ok());
  ASSERT_FLOAT_EQ(last_progress, 1.0f);

  // Check outputs
  auto scenes = std::any_cast<std::vector<std::string>>(ctx.outputs["scenes"]);
  ASSERT_EQ(scenes.size(), 3u);
}

TEST(Pipeline, MockImageGenExecutes) {
  auto stage = create_mock_stage(TaskType::ImageGen);
  ASSERT_EQ(stage->name(), "MockImageGen");

  StageContext ctx;
  ctx.trace_id = "test-trace";
  ctx.cancel_token = CancelToken::create();
  ctx.inputs["scene_index"] = 2;

  auto result = stage->execute(ctx);
  ASSERT_TRUE(result.is_ok());

  auto path = std::any_cast<std::string>(ctx.outputs["image_path"]);
  ASSERT_NE(path.find("frame_2"), std::string::npos);
}

TEST(Pipeline, MockComposeExecutes) {
  auto stage = create_mock_stage(TaskType::Compose);
  ASSERT_EQ(stage->name(), "MockCompose");

  StageContext ctx;
  ctx.trace_id = "test-trace";
  ctx.cancel_token = CancelToken::create();

  auto result = stage->execute(ctx);
  ASSERT_TRUE(result.is_ok());

  auto path = std::any_cast<std::string>(ctx.outputs["output_path"]);
  ASSERT_FALSE(path.empty());
}

TEST(Pipeline, CancelDuringExecution) {
  auto stage = create_mock_stage(TaskType::Storyboard);

  StageContext ctx;
  ctx.trace_id = "test-cancel";
  ctx.cancel_token = CancelToken::create();

  // Cancel immediately
  ctx.cancel_token->request_cancel();

  auto result = stage->execute(ctx);
  ASSERT_TRUE(result.is_err());
  ASSERT_EQ(result.error().category, ErrorCategory::Canceled);
}

TEST(Pipeline, ProgressCallbackInvoked) {
  auto stage = create_mock_stage(TaskType::ImageGen);

  StageContext ctx;
  ctx.trace_id = "test-progress";
  ctx.cancel_token = CancelToken::create();

  int progress_calls = 0;
  ctx.on_progress = [&](float) { progress_calls++; };

  stage->execute(ctx);
  ASSERT_GT(progress_calls, 0);
}

TEST(Pipeline, StageContextTypedIO) {
  StageContext ctx;
  ctx.set_output("count", 42);
  ctx.set_output("name", std::string("test"));

  // Retrieve via inputs (simulate inter-stage transfer)
  ctx.inputs = ctx.outputs;

  ASSERT_EQ(ctx.get_input<int>("count"), 42);
  ASSERT_EQ(ctx.get_input<std::string>("name"), "test");
  ASSERT_EQ(ctx.get_input<int>("missing", -1), -1);
}
