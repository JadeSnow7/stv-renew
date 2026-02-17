#pragma once

#include <QObject>
#include <QString>

namespace stv::app {

class StoryboardPresenter : public QObject {
  Q_OBJECT
  Q_PROPERTY(int sceneCount READ sceneCount NOTIFY sceneCountChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
  explicit StoryboardPresenter(QObject *parent = nullptr);

  [[nodiscard]] int sceneCount() const { return scene_count_; }
  [[nodiscard]] QString lastError() const { return last_error_; }

  Q_INVOKABLE void setSceneCount(int count);
  Q_INVOKABLE void saveStoryboard();

signals:
  void sceneCountChanged();
  void lastErrorChanged();
  void storyboardSaved();

private:
  int scene_count_ = 0;
  QString last_error_;
};

} // namespace stv::app
