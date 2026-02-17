#include "app/asset_presenter.h"
#include "app/auth_presenter.h"
#include "app/export_presenter.h"
#include "app/job_presenter.h"
#include "app/presenter.h"
#include "app/project_presenter.h"
#include "app/storyboard_presenter.h"
#include "core/scheduler.h"
#include "infra/logger.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include <memory>

namespace stv::core {
std::unique_ptr<IScheduler> create_simple_scheduler();
}
namespace stv::infra {
std::unique_ptr<ILogger> create_console_logger();
}

int main(int argc, char *argv[]) {
  QGuiApplication qtapp(argc, argv);
  QQuickStyle::setStyle("Basic");

  auto logger = stv::infra::create_console_logger();
  auto logger_ptr = std::shared_ptr<stv::infra::ILogger>(logger.release());

  auto scheduler = stv::core::create_simple_scheduler();
  auto scheduler_ptr = std::shared_ptr<stv::core::IScheduler>(scheduler.release());

  auto *presenter = new stv::app::Presenter(scheduler_ptr, logger_ptr);
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
