#include "infra/logger.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>

namespace stv::infra {

class ConsoleLogger final : public ILogger {
public:
  ConsoleLogger() {
    logger_ = spdlog::get("stv");
    if (!logger_) {
      logger_ = spdlog::stdout_color_mt("stv");
    }
    logger_->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%^%l%$] %v");
  }

  void info(const std::string &trace_id, const std::string &component,
            const std::string &event, const std::string &msg) override {
    logger_->info("[{}] [{}] {}: {}", trace_id, component, event, msg);
  }

  void warn(const std::string &trace_id, const std::string &component,
            const std::string &event, const std::string &msg) override {
    logger_->warn("[{}] [{}] {}: {}", trace_id, component, event, msg);
  }

  void error(const std::string &trace_id, const std::string &component,
             const std::string &event, const std::string &msg) override {
    logger_->error("[{}] [{}] {}: {}", trace_id, component, event, msg);
  }

private:
  std::shared_ptr<spdlog::logger> logger_;
};

std::unique_ptr<ILogger> create_console_logger() {
  return std::make_unique<ConsoleLogger>();
}

} // namespace stv::infra
