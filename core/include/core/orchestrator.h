#pragma once

#include "core/pipeline.h"
#include "core/result.h"
#include "core/scheduler.h"
#include "core/task.h"
#include "core/task_error.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace stv {
namespace infra {
class ILogger;
}

namespace core {

/// WorkflowEngine — orchestrates the creation and submission of a linked
/// task chain for a single story-to-video workflow.
///
/// Responsibilities:
///   1. Accept user input (story text, style)
///   2. Create TaskDescriptors with correct dependencies
///   3. Create and assign IStage implementations to each task
///   4. Submit all tasks to the scheduler
///   5. Listen for state changes and propagate to the presenter
///
/// Does NOT execute tasks — that's the scheduler's job.
class WorkflowEngine {
public:
  WorkflowEngine(std::shared_ptr<IScheduler> scheduler,
                 std::shared_ptr<infra::ILogger> logger);

  /// Workflow completion callback.
  /// Parameters: trace_id, success, output_path (empty on failure)
  using CompletionCallback =
      std::function<void(const std::string &trace_id, bool success,
                         const std::string &output_path)>;

  /// Per-task progress callback.
  /// Parameters: trace_id, task_id, state, progress
  using ProgressCallback = std::function<void(const std::string &trace_id,
                                              const std::string &task_id,
                                              TaskState state, float progress)>;

  /// Register a callback for workflow completion.
  void on_completion(CompletionCallback cb);

  /// Register a callback for per-task progress updates.
  void on_progress(ProgressCallback cb);

  /// Start a new workflow.
  /// Creates: Storyboard → ImageGen×N → Compose task chain.
  /// Returns the trace_id for this workflow.
  std::string start_workflow(const std::string &story_text,
                             const std::string &style, int scene_count = 4);

  /// Cancel an entire workflow by trace_id.
  Result<void, TaskError> cancel_workflow(const std::string &trace_id);

  /// Register a stage factory for a given task type.
  /// Allows swapping mock stages for real implementations.
  using StageFactory = std::function<std::shared_ptr<IStage>(TaskType)>;
  void set_stage_factory(StageFactory factory);

private:
  std::shared_ptr<IScheduler> scheduler_;
  std::shared_ptr<infra::ILogger> logger_;
  CompletionCallback completion_cb_;
  ProgressCallback progress_cb_;
  StageFactory stage_factory_;

  /// Track tasks per workflow for cancellation and completion detection.
  struct WorkflowState {
    std::string trace_id;
    std::vector<std::string> task_ids;
    int completed = 0;
    int total = 0;
    bool failed = false;
    std::string output_path;
  };
  std::vector<WorkflowState> active_workflows_;

  void handle_state_change(const std::string &task_id, TaskState state,
                           float progress);
  std::string generate_uuid();
};

} // namespace core
} // namespace stv
