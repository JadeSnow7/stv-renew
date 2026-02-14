#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace stv::core {

/// Thread-safe cancellation token.
///
/// Design: single-writer (whoever calls request_cancel()) / multi-reader
/// (stages check is_canceled()). Uses atomic<bool> with acquire/release
/// semantics — no mutex needed for the flag itself.
///
/// Usage in pipeline stages:
///   void execute(StageContext& ctx) {
///       for (int i = 0; i < N; ++i) {
///           ctx.cancel_token->throw_if_canceled();
///           // ... do work ...
///       }
///   }
class CancelToken {
public:
  CancelToken() = default;

  /// Request cancellation. Thread-safe, idempotent.
  void request_cancel() noexcept;

  /// Check if cancellation has been requested.
  [[nodiscard]] bool is_canceled() const noexcept;

  /// Throw TaskError::Canceled if cancellation was requested.
  /// Call this at checkpoint positions in stage execution.
  void throw_if_canceled() const;

  /// Register a callback to be invoked when cancellation is requested.
  /// Callbacks are invoked synchronously from request_cancel().
  using Callback = std::function<void()>;
  void on_cancel(Callback cb);

  /// Create a shared CancelToken.
  static std::shared_ptr<CancelToken> create();

private:
  std::atomic<bool> canceled_{false};
  std::mutex cb_mutex_;
  std::vector<Callback> callbacks_;
};

} // namespace stv::core
