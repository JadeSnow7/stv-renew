#include "app/presenter.h"

#include "core/orchestrator.h"
#include "core/scheduler.h"

#include <QDateTime>

#include <cmath>

namespace stv::app {

Presenter::Presenter(std::shared_ptr<stv::core::IScheduler> scheduler,
                     std::shared_ptr<stv::core::ILogger> logger,
                     QObject *parent)
    : QObject(parent), scheduler_(std::move(scheduler)),
      logger_(std::move(logger)) {
  engine_ = std::make_shared<stv::core::WorkflowEngine>(scheduler_, logger_);

  engine_->on_completion([this](const std::string & /*trace_id*/, bool success,
                                const std::string &output_path) {
    QMetaObject::invokeMethod(
        this,
        [this, success, output_path]() {
          if (success) {
            output_path_ = QString::fromStdString(output_path);
            emit outputPathChanged();
            setStatusText("Generation completed");
            appendLog("=== Workflow completed successfully ===");
            appendLog("Output: " + output_path_);
          } else {
            setStatusText("Generation failed");
            appendLog("=== Workflow failed ===");
          }
          setBusy(false);
          setProgress(1.0f);
          emit generationCompleted(success, output_path_);
        },
        Qt::QueuedConnection);
  });

  engine_->on_progress([this](const std::string & /*trace_id*/,
                              const std::string &task_id,
                              stv::core::TaskState state, float prog) {
    QMetaObject::invokeMethod(
        this,
        [this, task_id, state, prog]() {
          setStatusText(
              QString("[%1] %2 (%3%)")
                  .arg(QString::fromStdString(stv::core::to_string(state)))
                  .arg(QString::fromStdString(task_id).left(8))
                  .arg(static_cast<int>(std::round(prog * 100))));

          setProgress(prog);

          appendLog(
              QString("[%1] task=%2 state=%3 progress=%4%")
                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
                  .arg(QString::fromStdString(task_id).left(8))
                  .arg(QString::fromStdString(stv::core::to_string(state)))
                  .arg(QString::number(static_cast<double>(prog * 100.0), 'f',
                                       1)));
        },
        Qt::QueuedConnection);
  });

  tick_timer_ = new QTimer(this);
  tick_timer_->setInterval(50);
  connect(tick_timer_, &QTimer::timeout, this, &Presenter::onTick);
}

void Presenter::startGeneration(const QString &storyText, const QString &style,
                                int sceneCount) {
  if (busy_) {
    appendLog("Already generating; ignoring request.");
    return;
  }

  setBusy(true);
  setProgress(0.0f);
  setStatusText("Starting generation...");
  log_text_.clear();
  emit logTextChanged();
  output_path_.clear();
  emit outputPathChanged();

  appendLog("=== Starting new workflow ===");
  appendLog("Story: " + storyText.left(50) +
            (storyText.length() > 50 ? "..." : ""));
  appendLog("Style: " + style);
  appendLog("Scenes: " + QString::number(sceneCount));

  std::string trace_id = engine_->start_workflow(
      storyText.toStdString(), style.toStdString(), sceneCount);
  current_trace_id_ = QString::fromStdString(trace_id);
  appendLog("trace_id: " + current_trace_id_);

  tick_timer_->start();
}

void Presenter::set_stage_factory(stv::core::WorkflowEngine::StageFactory factory) {
  engine_->set_stage_factory(std::move(factory));
}

void Presenter::cancelGeneration() {
  if (!busy_ || current_trace_id_.isEmpty()) {
    return;
  }

  appendLog("Canceling workflow...");
  engine_->cancel_workflow(current_trace_id_.toStdString());
  setStatusText("Canceling...");
}

void Presenter::onTick() {
  scheduler_->tick();
  if (!scheduler_->has_pending_tasks()) {
    tick_timer_->stop();
  }
}

void Presenter::appendLog(const QString &line) {
  log_text_ += line + "\n";
  emit logTextChanged();
}

void Presenter::setBusy(bool b) {
  if (busy_ != b) {
    busy_ = b;
    emit busyChanged();
  }
}

void Presenter::setProgress(float p) {
  if (std::abs(progress_ - p) > 0.001f) {
    progress_ = p;
    emit progressChanged();
  }
}

void Presenter::setStatusText(const QString &s) {
  if (status_text_ != s) {
    status_text_ = s;
    emit statusTextChanged();
  }
}

} // namespace stv::app
