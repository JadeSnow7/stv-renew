#pragma once

#include <QObject>
#include <QString>

namespace stv::app {

class ExportPresenter : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString exportStatus READ exportStatus NOTIFY exportStatusChanged)
  Q_PROPERTY(QString downloadUrl READ downloadUrl NOTIFY downloadUrlChanged)

public:
  explicit ExportPresenter(QObject *parent = nullptr);

  [[nodiscard]] QString exportStatus() const { return export_status_; }
  [[nodiscard]] QString downloadUrl() const { return download_url_; }

  Q_INVOKABLE void createExport(const QString &projectId);

signals:
  void exportStatusChanged();
  void downloadUrlChanged();

private:
  QString export_status_ = "idle";
  QString download_url_;
};

} // namespace stv::app
