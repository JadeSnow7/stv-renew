#pragma once

#include "core/cancel_token.h"
#include "core/result.h"
#include "core/task_error.h"

#include <any>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace stv::core {

/// Context passed to each pipeline stage during execution.
/// Carries inputs/outputs, cancel token, and progress callback.
struct StageContext {
  std::string trace_id;
  std::shared_ptr<CancelToken> cancel_token;

  /// Key-value inputs from orchestrator or previous stages.
  std::unordered_map<std::string, std::any> inputs;

  /// Key-value outputs produced by this stage.
  std::unordered_map<std::string, std::any> outputs;

  /// Progress callback [0.0, 1.0]. Called by stage to report progress.
  std::function<void(float)> on_progress;

  /// Convenience: get typed input or return default.
  template <typename T>
  T get_input(const std::string &key, T default_val = T{}) const {
    auto it = inputs.find(key);
    if (it == inputs.end())
      return default_val;
    try {
      return std::any_cast<T>(it->second);
    } catch (const std::bad_any_cast &) {
      return default_val;
    }
  }

  /// Convenience: set typed output.
  template <typename T> void set_output(const std::string &key, T value) {
    outputs[key] = std::move(value);
  }
};

/// Abstract interface for a pipeline stage.
/// Each stage is a unit of work (e.g., storyboard generation, image gen,
/// compose).
class IStage {
public:
  virtual ~IStage() = default;

  /// Human-readable name for logging (e.g., "MockStoryboard").
  [[nodiscard]] virtual std::string name() const = 0;

  /// Execute the stage with the given context.
  /// Must check cancel_token at regular intervals.
  /// Returns Err on failure or cancellation.
  virtual Result<void, TaskError> execute(StageContext &ctx) = 0;
};

} // namespace stv::core
