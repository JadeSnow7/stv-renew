#include "core/cancel_token.h"
#include "core/task_error.h"

#include <stdexcept>

namespace stv::core {

void CancelToken::request_cancel() noexcept {
  bool expected = false;
  if (canceled_.compare_exchange_strong(expected, true,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
    // First cancellation — invoke callbacks
    std::lock_guard<std::mutex> lock(cb_mutex_);
    for (auto &cb : callbacks_) {
      if (cb) {
        try {
          cb();
        } catch (...) { /* swallow — cancel must not throw */
        }
      }
    }
  }
}

bool CancelToken::is_canceled() const noexcept {
  return canceled_.load(std::memory_order_acquire);
}

void CancelToken::throw_if_canceled() const {
  if (is_canceled()) {
    throw std::runtime_error("CancelToken: operation canceled");
  }
}

void CancelToken::on_cancel(Callback cb) {
  if (is_canceled()) {
    // Already canceled — invoke immediately
    if (cb)
      cb();
    return;
  }
  std::lock_guard<std::mutex> lock(cb_mutex_);
  callbacks_.push_back(std::move(cb));
}

std::shared_ptr<CancelToken> CancelToken::create() {
  return std::make_shared<CancelToken>();
}

} // namespace stv::core
