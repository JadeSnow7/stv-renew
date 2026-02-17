#pragma once

#include <QObject>
#include <QString>

#include "infra/path_service.h"
#include "infra/token_storage.h"

#include <memory>

namespace stv::app {

class AuthPresenter : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
  Q_PROPERTY(QString userEmail READ userEmail NOTIFY userEmailChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
  explicit AuthPresenter(QObject *parent = nullptr);

  [[nodiscard]] bool loggedIn() const { return logged_in_; }
  [[nodiscard]] QString userEmail() const { return user_email_; }
  [[nodiscard]] QString lastError() const { return last_error_; }

  Q_INVOKABLE void login(const QString &email, const QString &password);
  Q_INVOKABLE void logout();

signals:
  void loggedInChanged();
  void userEmailChanged();
  void lastErrorChanged();

private:
  bool logged_in_ = false;
  QString user_email_;
  QString last_error_;
  std::unique_ptr<stv::infra::PathService> path_service_;
  std::unique_ptr<stv::infra::TokenStorage> token_storage_;
};

} // namespace stv::app
