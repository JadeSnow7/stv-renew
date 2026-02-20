#pragma once

#include <string>

namespace stv::core {

/// Logger interface used by core and app-facing orchestration code.
/// Concrete implementations live in infra.
class ILogger {
public:
  virtual ~ILogger() = default;

  virtual void info(const std::string &trace_id, const std::string &component,
                    const std::string &event, const std::string &msg) = 0;

  virtual void warn(const std::string &trace_id, const std::string &component,
                    const std::string &event, const std::string &msg) = 0;

  virtual void error(const std::string &trace_id, const std::string &component,
                     const std::string &event, const std::string &msg) = 0;
};

} // namespace stv::core
