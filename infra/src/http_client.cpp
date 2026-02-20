#include "infra/http_client.h"

#include <charconv>
#include <thread>

namespace stv::infra {

namespace {
HttpErrorCode parse_http_error_code(const stv::core::TaskError &error) {
  const auto it = error.details.find("http_error_code");
  if (it == error.details.end()) {
    return HttpErrorCode::UNKNOWN;
  }

  int parsed = 0;
  const std::string &value = it->second;
  auto [ptr, ec] =
      std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (ec != std::errc() || ptr != value.data() + value.size()) {
    return HttpErrorCode::UNKNOWN;
  }
  return static_cast<HttpErrorCode>(parsed);
}
} // namespace

stv::core::TaskError make_http_error(
    HttpErrorCode code,
    const std::string &user_message,
    const std::string &internal_message,
    bool retryable
) {
  stv::core::ErrorCategory category = stv::core::ErrorCategory::Unknown;
  switch (code) {
  case HttpErrorCode::NETWORK_ERROR:
    category = stv::core::ErrorCategory::Network;
    break;
  case HttpErrorCode::TIMEOUT:
    category = stv::core::ErrorCategory::Timeout;
    break;
  case HttpErrorCode::CANCELED:
    category = stv::core::ErrorCategory::Canceled;
    break;
  case HttpErrorCode::SERVER_ERROR:
  case HttpErrorCode::RATE_LIMIT:
    category = stv::core::ErrorCategory::Network;
    break;
  case HttpErrorCode::CLIENT_ERROR:
  case HttpErrorCode::PARSE_ERROR:
    category = stv::core::ErrorCategory::Pipeline;
    break;
  case HttpErrorCode::UNKNOWN:
    category = stv::core::ErrorCategory::Unknown;
    break;
  }

  std::map<std::string, std::string> details = {
      {"http_error_code", std::to_string(static_cast<int>(code))}};

  return stv::core::TaskError(category, static_cast<int>(code), retryable,
                              user_message, internal_message, details);
}

RetryableHttpClient::RetryableHttpClient(
    std::shared_ptr<IHttpClient> inner,
    RetryPolicy policy,
    std::shared_ptr<stv::core::ILogger> logger)
    : inner_(std::move(inner)), policy_(std::move(policy)),
      logger_(std::move(logger)) {}

stv::core::Result<HttpResponse, stv::core::TaskError> RetryableHttpClient::execute(
    const HttpRequest &request,
    std::shared_ptr<stv::core::CancelToken> cancel_token
) {
  int retry_count = 0;
  auto backoff = policy_.initial_backoff;

  while (true) {
    if (cancel_token && cancel_token->is_canceled()) {
      auto err = make_http_error(HttpErrorCode::CANCELED, "Request canceled.",
                                 "Cancellation requested before HTTP call",
                                 false);
      err.details["retry_count"] = std::to_string(retry_count);
      err.details["request_id"] = request.request_id;
      return stv::core::Result<HttpResponse, stv::core::TaskError>::Err(err);
    }

    if (!inner_) {
      auto err = make_http_error(HttpErrorCode::UNKNOWN, "Request failed.",
                                 "RetryableHttpClient has null inner client",
                                 false);
      err.details["retry_count"] = std::to_string(retry_count);
      err.details["request_id"] = request.request_id;
      return stv::core::Result<HttpResponse, stv::core::TaskError>::Err(err);
    }

    auto result = inner_->execute(request, cancel_token);
    if (result.is_ok()) {
      return result;
    }

    auto error = result.error();
    const HttpErrorCode http_code = parse_http_error_code(error);
    const bool should_retry = policy_.should_retry(http_code);
    const bool has_attempts_left = retry_count < policy_.max_retries;

    error.details["retry_count"] = std::to_string(retry_count);
    if (!request.request_id.empty()) {
      error.details["request_id"] = request.request_id;
    }
    if (!should_retry || !has_attempts_left) {
      return stv::core::Result<HttpResponse, stv::core::TaskError>::Err(
          std::move(error));
    }

    if (logger_) {
      logger_->warn(
          request.trace_id, "http_client", "retry_scheduled",
          "request_id=" + request.request_id +
              " retry_count=" + std::to_string(retry_count + 1) +
              " max_retries=" + std::to_string(policy_.max_retries) +
              " backoff_ms=" + std::to_string(backoff.count()));
    }

    auto sleep_until = std::chrono::steady_clock::now() + backoff;
    while (std::chrono::steady_clock::now() < sleep_until) {
      if (cancel_token && cancel_token->is_canceled()) {
        auto canceled = make_http_error(
            HttpErrorCode::CANCELED, "Request canceled.",
            "Cancellation requested during retry backoff", false);
        canceled.details["retry_count"] = std::to_string(retry_count);
        canceled.details["request_id"] = request.request_id;
        return stv::core::Result<HttpResponse, stv::core::TaskError>::Err(
            std::move(canceled));
      }
      std::this_thread::sleep_for(policy_.sleep_slice);
    }

    auto next_backoff = std::chrono::duration_cast<std::chrono::milliseconds>(
        backoff * policy_.backoff_multiplier);
    backoff = std::min(next_backoff, policy_.max_backoff);
    retry_count++;
  }
}

bool RetryableHttpClient::cancel(const std::string &request_id) {
  return inner_ ? inner_->cancel(request_id) : false;
}

} // namespace stv::infra
