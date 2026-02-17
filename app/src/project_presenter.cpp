#include "app/project_presenter.h"

namespace stv::app {

ProjectPresenter::ProjectPresenter(QObject *parent) : QObject(parent) {}

void ProjectPresenter::refreshProjects() {
  if (project_names_.isEmpty()) {
    project_names_ << "Demo Project";
    emit projectNamesChanged();
  }
}

void ProjectPresenter::createProject(const QString &name) {
  if (name.trimmed().isEmpty()) {
    last_error_ = "project name required";
    emit lastErrorChanged();
    return;
  }
  project_names_ << name.trimmed();
  last_error_.clear();
  emit projectNamesChanged();
  emit lastErrorChanged();
}

} // namespace stv::app
