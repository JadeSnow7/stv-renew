#include "core/orchestrator.h"
#include "infra/logger.h"

#include <algorithm>
#include <random>
#include <sstream>

namespace stv::core {

// Forward declaration of mock stage factory
std::shared_ptr<IStage> create_mock_stage(TaskType type);

WorkflowEngine::WorkflowEngine(std::shared_ptr<IScheduler> scheduler,
                               std::shared_ptr<infra::ILogger> logger)
    : scheduler_(std::move(scheduler)), logger_(std::move(logger)),
      stage_factory_(create_mock_stage) // Default: use mock stages
{
  // Register for scheduler state changes
  scheduler_->on_state_change(
      [this](const std::string &task_id, TaskState state, float progress) {
        handle_state_change(task_id, state, progress);
      });
}

void WorkflowEngine::on_completion(CompletionCallback cb) {
  completion_cb_ = std::move(cb);
}

void WorkflowEngine::on_progress(ProgressCallback cb) {
  progress_cb_ = std::move(cb);
}

void WorkflowEngine::set_stage_factory(StageFactory factory) {
  stage_factory_ = std::move(factory);
}

std::string WorkflowEngine::start_workflow(const std::string & /*story_text*/,
                                           const std::string &style,
                                           int scene_count) {
  std::string trace_id = generate_uuid();

  if (logger_) {
    logger_->info(trace_id, "orchestrator", "workflow_start",
                  "Starting workflow: scenes=" + std::to_string(scene_count) +
                      " style=" + style);
  }

  // Shared cancel token for the entire workflow
  auto workflow_cancel = CancelToken::create();

  WorkflowState wf;
  wf.trace_id = trace_id;

  // ---- Task 1: Storyboard Generation ----
  TaskDescriptor storyboard_task;
  storyboard_task.task_id = generate_uuid();
  storyboard_task.trace_id = trace_id;
  storyboard_task.type = TaskType::Storyboard;
  storyboard_task.priority = 100; // Highest priority
  storyboard_task.cancel_token = workflow_cancel;
  // No deps — starts immediately
  wf.task_ids.push_back(storyboard_task.task_id);

  auto storyboard_stage = stage_factory_(TaskType::Storyboard);
  scheduler_->submit(std::move(storyboard_task), storyboard_stage);

  // ---- Tasks 2..N+1: Image Generation (one per scene) ----
  std::vector<std::string> image_task_ids;
  for (int i = 0; i < scene_count; ++i) {
    TaskDescriptor img_task;
    img_task.task_id = generate_uuid();
    img_task.trace_id = trace_id;
    img_task.type = TaskType::ImageGen;
    img_task.priority = 50;
    img_task.cancel_token = workflow_cancel;
    img_task.deps = {wf.task_ids[0]}; // Depends on storyboard

    image_task_ids.push_back(img_task.task_id);
    wf.task_ids.push_back(img_task.task_id);

    auto img_stage = stage_factory_(TaskType::ImageGen);
    scheduler_->submit(std::move(img_task), img_stage);
  }

  // ---- Task N+2: Compose (depends on all image tasks) ----
  TaskDescriptor compose_task;
  compose_task.task_id = generate_uuid();
  compose_task.trace_id = trace_id;
  compose_task.type = TaskType::Compose;
  compose_task.priority = 10;
  compose_task.cancel_token = workflow_cancel;
  compose_task.deps = image_task_ids; // Depends on all images

  wf.task_ids.push_back(compose_task.task_id);
  wf.total = static_cast<int>(wf.task_ids.size());

  auto compose_stage = stage_factory_(TaskType::Compose);
  scheduler_->submit(std::move(compose_task), compose_stage);

  active_workflows_.push_back(std::move(wf));

  if (logger_) {
    logger_->info(trace_id, "orchestrator", "workflow_created",
                  "Tasks created: " + std::to_string(wf.total) +
                      " (1 storyboard + " + std::to_string(scene_count) +
                      " images + 1 compose)");
  }

  return trace_id;
}

Result<void, TaskError>
WorkflowEngine::cancel_workflow(const std::string &trace_id) {
  auto it = std::find_if(
      active_workflows_.begin(), active_workflows_.end(),
      [&trace_id](const WorkflowState &wf) { return wf.trace_id == trace_id; });

  if (it == active_workflows_.end()) {
    return Result<void, TaskError>::Err(
        TaskError::Internal("Workflow not found: " + trace_id));
  }

  if (logger_) {
    logger_->info(trace_id, "orchestrator", "workflow_cancel",
                  "Canceling workflow");
  }

  for (const auto &task_id : it->task_ids) {
    scheduler_->cancel(task_id); // Best-effort cancel
  }

  return Result<void, TaskError>::Ok();
}

void WorkflowEngine::handle_state_change(const std::string &task_id,
                                         TaskState state, float progress) {
  // Find which workflow this task belongs to
  for (auto &wf : active_workflows_) {
    auto task_it = std::find(wf.task_ids.begin(), wf.task_ids.end(), task_id);
    if (task_it == wf.task_ids.end())
      continue;

    // Forward per-task progress
    if (progress_cb_) {
      progress_cb_(wf.trace_id, task_id, state, progress);
    }

    if (logger_) {
      logger_->info(wf.trace_id, "orchestrator", "task_state_changed",
                    "task_id=" + task_id + " state=" + to_string(state) +
                        " progress=" + std::to_string(progress));
    }

    if (state == TaskState::Succeeded) {
      wf.completed++;
    } else if (state == TaskState::Failed || state == TaskState::Canceled) {
      wf.failed = true;
    }

    // Check workflow completion
    if (wf.completed == wf.total) {
      // All tasks succeeded
      wf.output_path = "/tmp/stv_mock/final_output.mp4";
      if (completion_cb_) {
        completion_cb_(wf.trace_id, true, wf.output_path);
      }
      if (logger_) {
        logger_->info(wf.trace_id, "orchestrator", "workflow_completed",
                      "All tasks succeeded. Output: " + wf.output_path);
      }
    } else if (wf.failed && !is_terminal(state)) {
      // A task failed — workflow is done
    } else if (wf.failed && is_terminal(state)) {
      // All tasks terminal — workflow failed
      // For now, let the next tick handle cleanup
    }

    return; // Found the workflow
  }
}

std::string WorkflowEngine::generate_uuid() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<uint32_t> dis;

  std::stringstream ss;
  ss << std::hex;
  ss << ((dis(gen) & 0xFFFF0000) | 0x0000FFFF);
  ss << "-";
  ss << (dis(gen) & 0xFFFF);
  ss << "-4"; // Version 4
  ss << (dis(gen) & 0x0FFF);
  ss << "-";
  ss << ((dis(gen) & 0x3FFF) | 0x8000);
  ss << "-";
  ss << (dis(gen) & 0xFFFF);
  ss << (dis(gen) & 0xFFFF);
  ss << (dis(gen) & 0xFFFF);
  return ss.str();
}

} // namespace stv::core
