#include "infra/sse_client.h"

namespace stv::infra {

stv::core::Result<void, ApiError> SseJobEventStream::subscribe(
    const std::string &job_id,
    std::function<void(const stv::core::remote::JobEvent &)> /*on_event*/) {
  if (job_id.empty()) {
    return stv::core::Result<void, ApiError>::Err(
        ApiError{400, "INVALID_JOB_ID", false, "job_id is empty", ""});
  }
  return stv::core::Result<void, ApiError>::Err(
      ApiError{501, "SSE_NOT_IMPLEMENTED", true,
               "SSE stream parser/reconnect is pending implementation", ""});
}

void SseJobEventStream::unsubscribe(const std::string & /*job_id*/) {}

} // namespace stv::infra
