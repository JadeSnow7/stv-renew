#include "infra/logger.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>

namespace stv::infra {

#ifdef SPDLOG_DISABLED
/// Simple thread-safe logger (fallback when spdlog is disabled)
class SimpleLogger : public stv::core::ILogger {
public:
  void info(const std::string &trace_id, const std::string &component,
            const std::string &event, const std::string &msg) override {
    log("INFO", trace_id, component, event, msg);
  }

  void warn(const std::string &trace_id, const std::string &component,
            const std::string &event, const std::string &msg) override {
    log("WARN", trace_id, component, event, msg);
  }

  void error(const std::string &trace_id, const std::string &component,
             const std::string &event, const std::string &msg) override {
    log("ERROR", trace_id, component, event, msg);
  }

private:
  std::mutex mutex_;

  void log(const char* level, const std::string &trace_id, const std::string &component,
           const std::string &event, const std::string &msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);

    std::cout << "[" << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << "] "
              << "[" << level << "] "
              << "[" << trace_id << "] "
              << "[" << component << "] "
              << event << ": " << msg << "\n";
  }
};

/// Factory function
std::unique_ptr<stv::core::ILogger> create_console_logger() {
  return std::make_unique<SimpleLogger>();
}

#else

/// ConsoleLogger — spdlog-based structured logger.
/// Format: [ts] [level] [trace_id] [component] event: msg
class ConsoleLogger : public stv::core::ILogger {
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

/// Factory function
std::unique_ptr<stv::core::ILogger> create_console_logger() {
  return std::make_unique<ConsoleLogger>();
}

#endif

} // namespace stv::infra
