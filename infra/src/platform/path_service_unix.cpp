#ifndef _WIN32

#include "infra/path_service.h"

#include <cstdlib>
#include <pwd.h>
#include <unistd.h>

#include <stdexcept>
#include <string>

namespace stv::infra {

namespace {

std::string home_dir() {
  const char *home = std::getenv("HOME");
  if (home != nullptr && home[0] != '\0') {
    return std::string(home);
  }
  passwd *pw = getpwuid(getuid());
  if (pw != nullptr && pw->pw_dir != nullptr) {
    return std::string(pw->pw_dir);
  }
  throw std::runtime_error("Unable to resolve HOME directory");
}

class PathServiceUnix final : public PathService {
public:
  [[nodiscard]] std::string config_dir() const override {
#ifdef __APPLE__
    return home_dir() + "/Library/Application Support/stv_renew";
#else
    const char *xdg = std::getenv("XDG_CONFIG_HOME");
    return xdg != nullptr && xdg[0] != '\0'
               ? std::string(xdg) + "/stv_renew"
               : home_dir() + "/.config/stv_renew";
#endif
  }

  [[nodiscard]] std::string cache_dir() const override {
#ifdef __APPLE__
    return home_dir() + "/Library/Caches/stv_renew";
#else
    const char *xdg = std::getenv("XDG_CACHE_HOME");
    return xdg != nullptr && xdg[0] != '\0'
               ? std::string(xdg) + "/stv_renew"
               : home_dir() + "/.cache/stv_renew";
#endif
  }

  [[nodiscard]] std::string data_dir() const override {
#ifdef __APPLE__
    return home_dir() + "/Library/Application Support/stv_renew/data";
#else
    const char *xdg = std::getenv("XDG_DATA_HOME");
    return xdg != nullptr && xdg[0] != '\0'
               ? std::string(xdg) + "/stv_renew"
               : home_dir() + "/.local/share/stv_renew";
#endif
  }

  [[nodiscard]] std::string download_dir() const override {
    return home_dir() + "/Downloads";
  }
};

} // namespace

std::unique_ptr<PathService> PathService::create() {
  return std::make_unique<PathServiceUnix>();
}

} // namespace stv::infra

#endif // !_WIN32
