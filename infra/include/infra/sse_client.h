#pragma once

#include "core/remote/dto.h"
#include "core/result.h"
#include "infra/api_client.h"

#include <functional>
#include <string>
#include <utility>

namespace stv::infra {

class IJobEventStream {
public:
  virtual ~IJobEventStream() = default;

  virtual stv::core::Result<void, ApiError>
  subscribe(const std::string &job_id,
            std::function<void(const stv::core::remote::JobEvent &)> on_event) = 0;

  virtual void unsubscribe(const std::string &job_id) = 0;
};

/// Baseline placeholder for SSE integration.
/// Full parser/reconnect logic can be layered in M2-B.
class SseJobEventStream final : public IJobEventStream {
public:
  explicit SseJobEventStream(std::string base_url) : base_url_(std::move(base_url)) {}

  stv::core::Result<void, ApiError>
  subscribe(const std::string &job_id,
            std::function<void(const stv::core::remote::JobEvent &)> on_event) override;

  void unsubscribe(const std::string &job_id) override;

private:
  std::string base_url_;
};

} // namespace stv::infra
