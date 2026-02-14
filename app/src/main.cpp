#include "app/presenter.h"
#include "core/scheduler.h"
#include "infra/logger.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include <memory>

// Forward declarations from implementation files
namespace stv::core {
std::unique_ptr<IScheduler> create_simple_scheduler();
}
namespace stv::infra {
std::unique_ptr<ILogger> create_console_logger();
}

int main(int argc, char *argv[]) {
  QGuiApplication qtapp(argc, argv);
  QQuickStyle::setStyle("Basic");

  // ---- Wire up the architecture ----
  // 1. Create logger (infra)
  auto logger = stv::infra::create_console_logger();
  auto logger_ptr = std::shared_ptr<stv::infra::ILogger>(logger.release());

  // 2. Create scheduler (core)
  auto scheduler = stv::core::create_simple_scheduler();
  auto scheduler_ptr =
      std::shared_ptr<stv::core::IScheduler>(scheduler.release());

  // 3. Create presenter (app — bridges core to Qt)
  auto *presenter = new stv::app::Presenter(scheduler_ptr, logger_ptr);

  // 4. Set up QML engine
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("presenter", presenter);

  const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreated, &qtapp,
      [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
          QCoreApplication::exit(-1);
      },
      Qt::QueuedConnection);
  engine.load(url);

  return qtapp.exec();
}
