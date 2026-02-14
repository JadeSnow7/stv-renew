#pragma once

#include "core/orchestrator.h"
#include "core/task.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>
#include <string>

namespace stv::core {
class IScheduler;
class WorkflowEngine;
} // namespace stv::core

namespace stv::infra {
class ILogger;
} // namespace stv::infra

namespace stv::app {

/// Presenter — thin QObject bridge between QML UI and core WorkflowEngine.
///
/// Responsibilities:
///   - Translate QML invocations to core API calls
///   - Translate core callbacks to Qt signals
///   - Drive scheduler tick via QTimer
///
/// Does NOT contain business logic — that lives in core.
class Presenter : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  Q_PROPERTY(float progress READ progress NOTIFY progressChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
  Q_PROPERTY(QString outputPath READ outputPath NOTIFY outputPathChanged)
  Q_PROPERTY(QString logText READ logText NOTIFY logTextChanged)

public:
  explicit Presenter(std::shared_ptr<stv::core::IScheduler> scheduler,
                     std::shared_ptr<stv::infra::ILogger> logger,
                     QObject *parent = nullptr);

  // ---- Properties ----
  [[nodiscard]] bool busy() const { return busy_; }
  [[nodiscard]] float progress() const { return progress_; }
  [[nodiscard]] QString statusText() const { return status_text_; }
  [[nodiscard]] QString outputPath() const { return output_path_; }
  [[nodiscard]] QString logText() const { return log_text_; }

  // ---- QML-invokable methods ----
  Q_INVOKABLE void startGeneration(const QString &storyText,
                                   const QString &style, int sceneCount);
  Q_INVOKABLE void cancelGeneration();

signals:
  void busyChanged();
  void progressChanged();
  void statusTextChanged();
  void outputPathChanged();
  void logTextChanged();
  void generationCompleted(bool success, const QString &path);

private slots:
  void onTick();

private:
  std::shared_ptr<stv::core::WorkflowEngine> engine_;
  std::shared_ptr<stv::core::IScheduler> scheduler_;
  std::shared_ptr<stv::infra::ILogger> logger_;

  QTimer *tick_timer_;
  QString current_trace_id_;

  bool busy_ = false;
  float progress_ = 0.0f;
  QString status_text_;
  QString output_path_;
  QString log_text_;

  void appendLog(const QString &line);
  void setBusy(bool b);
  void setProgress(float p);
  void setStatusText(const QString &s);
};

} // namespace stv::app
