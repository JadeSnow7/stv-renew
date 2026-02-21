#include "app/asset_presenter.h"
#include "app/auth_presenter.h"
#include "app/export_presenter.h"
#include "app/job_presenter.h"
#include "app/presenter.h"
#include "app/project_presenter.h"
#include "app/storyboard_presenter.h"
#include "core/logger.h"
#include "core/scheduler.h"
#include "infra/curl_http_client.h"
#include "infra/http_client.h"
#include "infra/logger.h"
#include "infra/stage_factory.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

namespace {

int auto_worker_count() {
  const auto hw = static_cast<int>(std::thread::hardware_concurrency());
  if (hw <= 0) {
    return 4;
  }
  return std::clamp(hw - 1, 2, 8);
}

int parse_env_int(const char *name, int fallback, bool allow_zero,
                  const std::shared_ptr<stv::core::ILogger> &logger) {
  const char *raw = std::getenv(name);
  if (!raw || raw[0] == 0) {
    return fallback;
  }

  char *end = nullptr;
  const long value = std::strtol(raw, &end, 10);
  const bool valid = end && *end == 0 && (allow_zero ? value >= 0 : value > 0);
  if (!valid) {
    if (logger) {
      logger->warn("startup", "app", "scheduler_config_invalid",
                   std::string("Invalid value for ") + name + "=" + raw +
                       ", fallback=" + std::to_string(fallback));
    }
    return fallback;
  }

  return static_cast<int>(value);
}

stv::core::SchedulerConfig build_scheduler_config(
    const std::shared_ptr<stv::core::ILogger> &logger) {
  stv::core::SchedulerConfig cfg;
  cfg.worker_count = auto_worker_count();
  cfg.resource_budget.cpu_slots_hard = cfg.worker_count;
  cfg.resource_budget.ram_soft_mb = 2048;
  cfg.resource_budget.vram_soft_mb = 7680;
  cfg.aging_policy.interval_ms = 500;
  cfg.aging_policy.boost_per_interval = 1;
  cfg.pause_policy.checkpoint_timeout_ms = 1500;

  cfg.worker_count = parse_env_int("STV_SCHED_WORKERS", cfg.worker_count, false,
                                   logger);
  cfg.resource_budget.cpu_slots_hard = parse_env_int(
      "STV_SCHED_CPU_SLOTS", cfg.resource_budget.cpu_slots_hard, false, logger);
  cfg.resource_budget.ram_soft_mb = parse_env_int(
      "STV_SCHED_RAM_MB_SOFT", cfg.resource_budget.ram_soft_mb, true, logger);
  cfg.resource_budget.vram_soft_mb = parse_env_int(
      "STV_SCHED_VRAM_MB_SOFT", cfg.resource_budget.vram_soft_mb, true, logger);
  cfg.aging_policy.interval_ms = parse_env_int(
      "STV_SCHED_AGING_INTERVAL_MS", cfg.aging_policy.interval_ms, false, logger);
  cfg.aging_policy.boost_per_interval = parse_env_int(
      "STV_SCHED_AGING_BOOST", cfg.aging_policy.boost_per_interval, false,
      logger);
  cfg.pause_policy.checkpoint_timeout_ms = parse_env_int(
      "STV_SCHED_PAUSE_TIMEOUT_MS", cfg.pause_policy.checkpoint_timeout_ms,
      false, logger);

  return cfg;
}

} // namespace

namespace stv::infra {
std::unique_ptr<stv::core::ILogger> create_console_logger();
} // namespace stv::infra

int main(int argc, char *argv[]) {
  QGuiApplication qtapp(argc, argv);
  QQuickStyle::setStyle("Basic");

  auto logger = stv::infra::create_console_logger();
  auto logger_ptr = std::shared_ptr<stv::core::ILogger>(logger.release());

  auto scheduler_config = build_scheduler_config(logger_ptr);
  const char *scheduler_env = std::getenv("STV_SCHEDULER");
  const std::string scheduler_mode =
      scheduler_env ? scheduler_env : std::string("threadpool");

  std::unique_ptr<stv::core::IScheduler> scheduler;
  if (scheduler_mode == "simple") {
    scheduler = stv::core::create_simple_scheduler();
    if (logger_ptr) {
      logger_ptr->warn("startup", "app", "scheduler_mode",
                       "Using simple scheduler fallback");
    }
  } else {
    if (scheduler_mode != "threadpool" && logger_ptr) {
      logger_ptr->warn("startup", "app", "scheduler_mode",
                       "Unknown STV_SCHEDULER value, fallback to threadpool: " +
                           scheduler_mode);
    }
    scheduler =
        stv::core::create_thread_pool_scheduler(scheduler_config, logger_ptr);
  }
  auto scheduler_ptr =
      std::shared_ptr<stv::core::IScheduler>(scheduler.release());

  // Create HTTP client with retry policy (infra)
  auto curl_client = std::make_shared<stv::infra::CurlHttpClient>();
  stv::infra::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.initial_backoff = std::chrono::milliseconds(500);
  retry_policy.max_backoff = std::chrono::milliseconds(5000);
  retry_policy.backoff_multiplier = 2.0;

  auto http_client = std::make_shared<stv::infra::RetryableHttpClient>(
      curl_client, retry_policy, logger_ptr);

  const char *api_base_env = std::getenv("STV_API_BASE_URL");
  std::string api_base_url =
      api_base_env ? api_base_env : "http://127.0.0.1:8765";

  auto stage_factory_obj =
      std::make_shared<stv::infra::StageFactory>(http_client, api_base_url);

  auto *presenter = new stv::app::Presenter(scheduler_ptr, logger_ptr);

  // Set stage factory to workflow engine
  presenter->set_stage_factory([stage_factory_obj](stv::core::TaskType type) {
    return stage_factory_obj->create_stage(type);
  });

  auto *auth_presenter = new stv::app::AuthPresenter();
  auto *project_presenter = new stv::app::ProjectPresenter();
  auto *storyboard_presenter = new stv::app::StoryboardPresenter();
  auto *job_presenter = new stv::app::JobPresenter();
  auto *asset_presenter = new stv::app::AssetPresenter();
  auto *export_presenter = new stv::app::ExportPresenter();

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("presenter", presenter);
  engine.rootContext()->setContextProperty("authPresenter", auth_presenter);
  engine.rootContext()->setContextProperty("projectPresenter", project_presenter);
  engine.rootContext()->setContextProperty("storyboardPresenter",
                                           storyboard_presenter);
  engine.rootContext()->setContextProperty("jobPresenter", job_presenter);
  engine.rootContext()->setContextProperty("assetPresenter", asset_presenter);
  engine.rootContext()->setContextProperty("exportPresenter", export_presenter);

  const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreated, &qtapp,
      [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
          QCoreApplication::exit(-1);
        }
      },
      Qt::QueuedConnection);
  engine.load(url);

  return qtapp.exec();
}
