#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace stv::app {

class ProjectPresenter : public QObject {
  Q_OBJECT
  Q_PROPERTY(QStringList projectNames READ projectNames NOTIFY projectNamesChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
  explicit ProjectPresenter(QObject *parent = nullptr);

  [[nodiscard]] QStringList projectNames() const { return project_names_; }
  [[nodiscard]] QString lastError() const { return last_error_; }

  Q_INVOKABLE void refreshProjects();
  Q_INVOKABLE void createProject(const QString &name);

signals:
  void projectNamesChanged();
  void lastErrorChanged();

private:
  QStringList project_names_;
  QString last_error_;
};

} // namespace stv::app
