#pragma once

#include <QObject>
#include <QStringList>

namespace stv::app {

class AssetPresenter : public QObject {
  Q_OBJECT
  Q_PROPERTY(QStringList assets READ assets NOTIFY assetsChanged)

public:
  explicit AssetPresenter(QObject *parent = nullptr);

  [[nodiscard]] QStringList assets() const { return assets_; }

  Q_INVOKABLE void refreshAssets(const QString &projectId);

signals:
  void assetsChanged();

private:
  QStringList assets_;
};

} // namespace stv::app
