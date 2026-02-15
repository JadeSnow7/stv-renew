#pragma once

#include <string>
#include <map>

namespace stv::core {

/// Error categories — enables programmatic branching on error type
/// without string parsing.
enum class ErrorCategory {
  Network,  // HTTP/connection failures
  Timeout,  // Deadline exceeded
  Resource, // OOM, disk full, GPU unavailable
  Pipeline, // Stage execution logic error
  Canceled, // User or system cancellation
  Internal, // Programming error / invariant violation
  Unknown
};

/// Structured error type for all task/pipeline operations.
/// M2 extension: added retryable, user_message/internal_message split, details map
struct TaskError {
  ErrorCategory category = ErrorCategory::Unknown;
  int code = 0;        // Numeric code for telemetry aggregation
  std::string message; // Human-readable detail (DEPRECATED: use user_message)

  // M2 additions:
  bool retryable = false;              // Can this error be retried?
  std::string user_message;            // User-friendly message for UI
  std::string internal_message;        // Technical details for logging
  std::map<std::string, std::string> details; // Additional context (e.g., "http_error_code": "1001")

  TaskError() = default;

  // Legacy constructor (for backward compatibility)
  TaskError(ErrorCategory cat, int c, std::string msg)
      : category(cat), code(c), message(std::move(msg)),
        user_message(message), internal_message(message) {}

  // M2 full constructor
  TaskError(ErrorCategory cat, int c, bool retry,
            std::string user_msg, std::string internal_msg,
            std::map<std::string, std::string> dets = {})
      : category(cat), code(c), message(user_msg),
        retryable(retry), user_message(std::move(user_msg)),
        internal_message(std::move(internal_msg)), details(std::move(dets)) {}

  /// Convenience constructors
  static TaskError Canceled(std::string msg = "Operation canceled") {
    return {ErrorCategory::Canceled, 1, std::move(msg)};
  }
  static TaskError Timeout(std::string msg = "Deadline exceeded") {
    return {ErrorCategory::Timeout, 2, std::move(msg)};
  }
  static TaskError Pipeline(std::string msg) {
    return {ErrorCategory::Pipeline, 3, std::move(msg)};
  }
  static TaskError Internal(std::string msg) {
    return {ErrorCategory::Internal, 4, std::move(msg)};
  }
};

/// Convert ErrorCategory to string for logging.
inline const char *to_string(ErrorCategory cat) {
  switch (cat) {
  case ErrorCategory::Network:
    return "Network";
  case ErrorCategory::Timeout:
    return "Timeout";
  case ErrorCategory::Resource:
    return "Resource";
  case ErrorCategory::Pipeline:
    return "Pipeline";
  case ErrorCategory::Canceled:
    return "Canceled";
  case ErrorCategory::Internal:
    return "Internal";
  case ErrorCategory::Unknown:
    return "Unknown";
  }
  return "Unknown";
}

} // namespace stv::core
