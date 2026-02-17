#include "app/export_presenter.h"

namespace stv::app {

ExportPresenter::ExportPresenter(QObject *parent) : QObject(parent) {}

void ExportPresenter::createExport(const QString &projectId) {
  if (projectId.trimmed().isEmpty()) {
    export_status_ = "failed";
    download_url_.clear();
  } else {
    export_status_ = "succeeded";
    download_url_ =
        QString("https://storage.local/users/demo/projects/%1/final_video/final.mp4")
            .arg(projectId);
  }
  emit exportStatusChanged();
  emit downloadUrlChanged();
}

} // namespace stv::app
