#include "app/auth_presenter.h"

#include "infra/path_service.h"
#include "infra/token_storage.h"

namespace stv::app {

AuthPresenter::AuthPresenter(QObject *parent) : QObject(parent) {
  try {
    path_service_ = stv::infra::PathService::create();
    token_storage_ = std::make_unique<stv::infra::TokenStorage>(*path_service_);

    const auto tokens = token_storage_->load();
    if (tokens.has_value()) {
      logged_in_ = true;
      user_email_ = "restored@local";
    }
  } catch (...) {
    last_error_ = "token storage init failed";
  }
}

void AuthPresenter::login(const QString &email, const QString &password) {
  if (email.trimmed().isEmpty() || password.isEmpty()) {
    last_error_ = "email/password required";
    emit lastErrorChanged();
    return;
  }

  // Phase 1 MVP: persist dummy tokens after successful local validation.
  if (token_storage_) {
    token_storage_->save("access_token_placeholder",
                         "refresh_token_placeholder");
  }

  user_email_ = email.trimmed();
  logged_in_ = true;
  last_error_.clear();
  emit userEmailChanged();
  emit loggedInChanged();
  emit lastErrorChanged();
}

void AuthPresenter::logout() {
  logged_in_ = false;
  user_email_.clear();
  last_error_.clear();
  if (token_storage_) {
    token_storage_->clear();
  }
  emit loggedInChanged();
  emit userEmailChanged();
  emit lastErrorChanged();
}

} // namespace stv::app
