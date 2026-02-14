#include "core/task.h"

#include <algorithm>

namespace stv::core {

const char *to_string(TaskState state) {
  switch (state) {
  case TaskState::Queued:
    return "Queued";
  case TaskState::Ready:
    return "Ready";
  case TaskState::Running:
    return "Running";
  case TaskState::Paused:
    return "Paused";
  case TaskState::Canceled:
    return "Canceled";
  case TaskState::Failed:
    return "Failed";
  case TaskState::Succeeded:
    return "Succeeded";
  }
  return "Unknown";
}

bool is_terminal(TaskState state) {
  switch (state) {
  case TaskState::Canceled:
  case TaskState::Failed:
  case TaskState::Succeeded:
    return true;
  default:
    return false;
  }
}

const char *to_string(TaskType type) {
  switch (type) {
  case TaskType::Storyboard:
    return "Storyboard";
  case TaskType::ImageGen:
    return "ImageGen";
  case TaskType::VideoClip:
    return "VideoClip";
  case TaskType::TTS:
    return "TTS";
  case TaskType::Compose:
    return "Compose";
  }
  return "Unknown";
}

Result<void, TaskError> TaskDescriptor::transition_to(TaskState new_state) {
  // Validate transition legality.
  // Table-driven approach for clarity and exhaustiveness.
  bool legal = false;

  switch (state) {
  case TaskState::Queued:
    legal = (new_state == TaskState::Ready || new_state == TaskState::Canceled);
    break;
  case TaskState::Ready:
    legal =
        (new_state == TaskState::Running || new_state == TaskState::Canceled);
    break;
  case TaskState::Running:
    legal =
        (new_state == TaskState::Paused || new_state == TaskState::Succeeded ||
         new_state == TaskState::Failed || new_state == TaskState::Canceled);
    break;
  case TaskState::Paused:
    legal =
        (new_state == TaskState::Running || new_state == TaskState::Canceled);
    break;
  case TaskState::Failed:
    // Only retry transition: Failed → Queued
    legal = (new_state == TaskState::Queued);
    break;
  case TaskState::Canceled:
  case TaskState::Succeeded:
    // Terminal states — no transitions allowed
    legal = false;
    break;
  }

  if (!legal) {
    return Result<void, TaskError>::Err(TaskError::Internal(
        std::string("Illegal state transition: ") + to_string(state) + " -> " +
        to_string(new_state) + " (task_id=" + task_id + ")"));
  }

  // Apply transition
  TaskState old_state = state;
  state = new_state;

  // Update lifecycle timestamps
  if (new_state == TaskState::Running && !started_at.has_value()) {
    started_at = Clock::now();
  }
  if (is_terminal(new_state)) {
    finished_at = Clock::now();
  }

  // Reset progress on retry
  if (old_state == TaskState::Failed && new_state == TaskState::Queued) {
    progress = 0.0f;
    error.reset();
    started_at.reset();
    finished_at.reset();
  }

  return Result<void, TaskError>::Ok();
}

void TaskDescriptor::set_progress(float p) {
  progress = std::clamp(p, 0.0f, 1.0f);
}

} // namespace stv::core
