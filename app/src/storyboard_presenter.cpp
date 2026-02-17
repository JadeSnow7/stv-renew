#include "app/storyboard_presenter.h"

namespace stv::app {

StoryboardPresenter::StoryboardPresenter(QObject *parent) : QObject(parent) {}

void StoryboardPresenter::setSceneCount(int count) {
  if (count < 1) {
    last_error_ = "scene_count must be >= 1";
    emit lastErrorChanged();
    return;
  }
  scene_count_ = count;
  last_error_.clear();
  emit sceneCountChanged();
  emit lastErrorChanged();
}

void StoryboardPresenter::saveStoryboard() {
  if (scene_count_ < 1) {
    last_error_ = "no scenes to save";
    emit lastErrorChanged();
    return;
  }
  last_error_.clear();
  emit lastErrorChanged();
  emit storyboardSaved();
}

} // namespace stv::app
