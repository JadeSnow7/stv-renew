#pragma once

#include <QObject>
#include <QString>

namespace stv::app {

class JobPresenter : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString jobId READ jobId NOTIFY jobIdChanged)
  Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
  explicit JobPresenter(QObject *parent = nullptr);

  [[nodiscard]] QString jobId() const { return job_id_; }
  [[nodiscard]] double progress() const { return progress_; }
  [[nodiscard]] QString status() const { return status_; }

  Q_INVOKABLE void startJob(const QString &projectId);
  Q_INVOKABLE void cancelJob();
  Q_INVOKABLE void retryJob();

signals:
  void jobIdChanged();
  void progressChanged();
  void statusChanged();

private:
  QString job_id_;
  double progress_ = 0.0;
  QString status_ = "idle";
};

} // namespace stv::app
