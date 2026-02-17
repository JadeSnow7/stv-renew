#include "app/asset_presenter.h"

namespace stv::app {

AssetPresenter::AssetPresenter(QObject *parent) : QObject(parent) {}

void AssetPresenter::refreshAssets(const QString &projectId) {
  assets_.clear();
  if (!projectId.trimmed().isEmpty()) {
    assets_ << "final_video.mp4" << "scene_1.png" << "scene_1.mp3";
  }
  emit assetsChanged();
}

} // namespace stv::app
