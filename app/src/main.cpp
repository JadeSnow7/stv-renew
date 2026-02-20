#include "app/asset_presenter.h"
#include "app/auth_presenter.h"
#include "app/export_presenter.h"
#include "app/job_presenter.h"
#include "app/presenter.h"
#include "app/project_presenter.h"
#include "app/storyboard_presenter.h"
#include "core/logger.h"
#include "core/scheduler.h"
#include "infra/logger.h"
#include "infra/http_client.h"
#include "infra/curl_http_client.h"
#include "infra/stage_factory.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include <memory>
#include <cstdlib>

namespace stv::core {
std::unique_ptr<IScheduler> create_simple_scheduler();
}
namespace stv::infra {
std::unique_ptr<stv::core::ILogger> create_console_logger();
}

int main(int argc, char *argv[]) {
  QGuiApplication qtapp(argc, argv);
  QQuickStyle::setStyle("Basic");

  auto logger = stv::infra::create_console_logger();
  auto logger_ptr = std::shared_ptr<stv::core::ILogger>(logger.release());

  auto scheduler = stv::core::create_simple_scheduler();
  auto scheduler_ptr = std::shared_ptr<stv::core::IScheduler>(scheduler.release());

  // Create HTTP client with retry policy (infra)
  auto curl_client = std::make_shared<stv::infra::CurlHttpClient>();
  stv::infra::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.initial_backoff = std::chrono::milliseconds(500);
  retry_policy.max_backoff = std::chrono::milliseconds(5000);
  retry_policy.backoff_multiplier = 2.0;

  auto http_client = std::make_shared<stv::infra::RetryableHttpClient>(
      curl_client, retry_policy, logger_ptr);

  const char* api_base_env = std::getenv("STV_API_BASE_URL");
  std::string api_base_url = api_base_env ? api_base_env : "http://127.0.0.1:8765";

  auto stage_factory_obj = std::make_shared<stv::infra::StageFactory>(
      http_client, api_base_url);

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
  engine.rootContext()->setContextProperty("storyboardPresenter", storyboard_presenter);
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
