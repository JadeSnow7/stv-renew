#pragma once

#include "core/cancel_token.h"
#include "core/result.h"
#include "core/task_error.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace stv::core {

// ---- Task State Enum ----

enum class TaskState {
  Queued,   // Waiting for dependencies to be satisfied
  Ready,    // All dependencies met, waiting for scheduler dispatch
  Running,  // Actively executing a pipeline stage
  Paused,   // Execution suspended by user
  Canceled, // Canceled by user or timeout (terminal)
  Failed,   // Stage execution error (terminal, but retryable)
  Succeeded // Completed successfully (terminal)
};

/// Convert TaskState to string for logging/serialization.
const char *to_string(TaskState state);

/// Check if a state is terminal (no further transitions except retry from
/// Failed).
bool is_terminal(TaskState state);

// ---- Task Type Enum ----

enum class TaskType {
  Storyboard, // LLM storyboard generation
  ImageGen,   // Text-to-image generation
  VideoClip,  // Image-to-video conversion
  TTS,        // Text-to-speech synthesis
  Compose     // FFmpeg final composition
};

const char *to_string(TaskType type);

// ---- Task Descriptor ----

/// Core data structure representing a single task in the system.
/// Owns its state machine — transitions are validated via transition_to().
struct TaskDescriptor {
  std::string task_id;  // Unique identifier (UUID)
  std::string trace_id; // Workflow-level correlation ID
  TaskType type;
  TaskState state = TaskState::Queued;
  int priority = 0;
  float progress = 0.0f; // [0.0, 1.0]

  // Dependency management
  std::vector<std::string> deps; // Prerequisite task IDs

  // Lifecycle tracking
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;
  TimePoint created_at = Clock::now();
  std::optional<TimePoint> started_at;
  std::optional<TimePoint> finished_at;

  // Error and cancellation
  std::optional<TaskError> error;
  std::shared_ptr<CancelToken> cancel_token;

  // ---- State Machine ----

  /// Attempt a state transition. Returns Err if the transition is illegal.
  /// Legal transitions:
  ///   Queued  → Ready, Canceled
  ///   Ready   → Running, Canceled
  ///   Running → Paused, Succeeded, Failed, Canceled
  ///   Paused  → Running, Canceled
  ///   Failed  → Queued  (retry)
  Result<void, TaskError> transition_to(TaskState new_state);

  /// Convenience: set progress (clamped to [0,1]).
  void set_progress(float p);
};

} // namespace stv::core
