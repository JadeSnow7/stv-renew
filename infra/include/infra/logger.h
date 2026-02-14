#pragma once

#include <string>

namespace stv::infra {

/// Logger interface — abstracts away the logging backend.
/// Allows injection of ConsoleLogger, spdlog, or a mock for testing.
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

} // namespace stv::infra
