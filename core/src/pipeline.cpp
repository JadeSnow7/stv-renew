#include "core/pipeline.h"
#include "core/task.h"

#include <chrono>
#include <thread>
#include <vector>

namespace stv::core {

// ============================================================
// Mock Pipeline Stages (M1)
// These simulate real work with sleep + progress updates.
// Replaced by real implementations in M2.
// ============================================================

/// Mock storyboard generation: takes story_text, outputs scene list.
class MockStoryboardStage : public IStage {
public:
  std::string name() const override { return "MockStoryboard"; }

  Result<void, TaskError> execute(StageContext &ctx) override {
    // Simulate LLM processing (500ms with progress)
    const int steps = 5;
    for (int i = 0; i < steps; ++i) {
      if (ctx.cancel_token && ctx.cancel_token->is_canceled()) {
        return Result<void, TaskError>::Err(TaskError::Canceled());
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      if (ctx.on_progress) {
        ctx.on_progress(static_cast<float>(i + 1) / steps);
      }
    }

    // Produce mock scene prompts
    int scene_count = ctx.get_input<int>("scene_count", 4);
    std::vector<std::string> scenes;
    for (int i = 0; i < scene_count; ++i) {
      scenes.push_back("mock_scene_prompt_" + std::to_string(i + 1));
    }
    ctx.set_output("scenes", scenes);
    ctx.set_output("storyboard_json", std::string("{\"scenes\": [\"mock\"]}"));

    return Result<void, TaskError>::Ok();
  }
};

/// Mock image generation: simulates generating one image per scene.
class MockImageGenStage : public IStage {
public:
  std::string name() const override { return "MockImageGen"; }

  Result<void, TaskError> execute(StageContext &ctx) override {
    // Simulate SD inference (300ms)
    const int steps = 3;
    for (int i = 0; i < steps; ++i) {
      if (ctx.cancel_token && ctx.cancel_token->is_canceled()) {
        return Result<void, TaskError>::Err(TaskError::Canceled());
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      if (ctx.on_progress) {
        ctx.on_progress(static_cast<float>(i + 1) / steps);
      }
    }

    int scene_index = ctx.get_input<int>("scene_index", 0);
    std::string mock_path =
        "/tmp/stv_mock/frame_" + std::to_string(scene_index) + ".png";
    ctx.set_output("image_path", mock_path);

    return Result<void, TaskError>::Ok();
  }
};

/// Mock video composition: simulates FFmpeg assembly.
class MockComposeStage : public IStage {
public:
  std::string name() const override { return "MockCompose"; }

  Result<void, TaskError> execute(StageContext &ctx) override {
    // Simulate FFmpeg composition (500ms)
    const int steps = 5;
    for (int i = 0; i < steps; ++i) {
      if (ctx.cancel_token && ctx.cancel_token->is_canceled()) {
        return Result<void, TaskError>::Err(TaskError::Canceled());
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      if (ctx.on_progress) {
        ctx.on_progress(static_cast<float>(i + 1) / steps);
      }
    }

    std::string output_path = "/tmp/stv_mock/final_output.mp4";
    ctx.set_output("output_path", output_path);

    return Result<void, TaskError>::Ok();
  }
};

// ---- Factory function for mock stages ----

std::shared_ptr<IStage> create_mock_stage(TaskType type) {
  switch (type) {
  case TaskType::Storyboard:
    return std::make_shared<MockStoryboardStage>();
  case TaskType::ImageGen:
    return std::make_shared<MockImageGenStage>();
  case TaskType::Compose:
    return std::make_shared<MockComposeStage>();
  case TaskType::TTS:
    // M1: TTS not implemented, use a pass-through
    return std::make_shared<MockStoryboardStage>();
  case TaskType::VideoClip:
    return std::make_shared<MockImageGenStage>();
  }
  return std::make_shared<MockStoryboardStage>();
}

} // namespace stv::core
