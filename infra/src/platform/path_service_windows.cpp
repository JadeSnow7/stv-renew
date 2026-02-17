#ifdef _WIN32

#include "infra/path_service.h"

#include <shlobj.h>
#include <windows.h>

#include <stdexcept>
#include <string>

namespace stv::infra {

namespace {

std::string wide_to_utf8(const std::wstring &input) {
  if (input.empty()) {
    return {};
  }
  int bytes = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, nullptr, 0,
                                  nullptr, nullptr);
  if (bytes <= 0) {
    throw std::runtime_error("WideCharToMultiByte failed");
  }
  std::string out(static_cast<size_t>(bytes), '\0');
  WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, out.data(), bytes, nullptr,
                      nullptr);
  if (!out.empty() && out.back() == '\0') {
    out.pop_back();
  }
  return out;
}

std::string known_folder(const KNOWNFOLDERID &id) {
  PWSTR wpath = nullptr;
  HRESULT hr = SHGetKnownFolderPath(id, 0, nullptr, &wpath);
  if (FAILED(hr) || wpath == nullptr) {
    throw std::runtime_error("SHGetKnownFolderPath failed");
  }
  std::wstring path(wpath);
  CoTaskMemFree(wpath);
  return wide_to_utf8(path);
}

class PathServiceWindows final : public PathService {
public:
  [[nodiscard]] std::string config_dir() const override {
    return known_folder(FOLDERID_RoamingAppData) + "\\stv_renew";
  }

  [[nodiscard]] std::string cache_dir() const override {
    return known_folder(FOLDERID_LocalAppData) + "\\stv_renew\\cache";
  }

  [[nodiscard]] std::string data_dir() const override {
    return known_folder(FOLDERID_LocalAppData) + "\\stv_renew\\data";
  }

  [[nodiscard]] std::string download_dir() const override {
    return known_folder(FOLDERID_Downloads);
  }
};

} // namespace

std::unique_ptr<PathService> PathService::create() {
  return std::make_unique<PathServiceWindows>();
}

} // namespace stv::infra

#endif // _WIN32
