#include "app/job_presenter.h"

namespace stv::app {

JobPresenter::JobPresenter(QObject *parent) : QObject(parent) {}

void JobPresenter::startJob(const QString &projectId) {
  if (projectId.trimmed().isEmpty()) {
    status_ = "invalid_project";
    emit statusChanged();
    return;
  }
  job_id_ = QString("job_%1").arg(projectId.left(8));
  progress_ = 0.01;
  status_ = "queued";
  emit jobIdChanged();
  emit progressChanged();
  emit statusChanged();
}

void JobPresenter::cancelJob() {
  status_ = "cancel_requested";
  emit statusChanged();
}

void JobPresenter::retryJob() {
  progress_ = 0.0;
  status_ = "retry_queued";
  emit progressChanged();
  emit statusChanged();
}

} // namespace stv::app
