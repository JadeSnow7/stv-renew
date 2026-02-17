#include "infra/token_storage.h"

#include "infra/path_service.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace stv::infra {

TokenStorage::TokenStorage(const PathService &path_service)
    : path_service_(path_service) {}

void TokenStorage::save(const std::string &access_token,
                        const std::string &refresh_token) {
  const std::filesystem::path file_path(token_file_path());
  std::filesystem::create_directories(file_path.parent_path());

  std::ofstream ofs(file_path, std::ios::trunc);
  ofs << access_token << '\n' << refresh_token << '\n';
}

std::optional<std::pair<std::string, std::string>> TokenStorage::load() const {
  const std::filesystem::path file_path(token_file_path());
  if (!std::filesystem::exists(file_path)) {
    return std::nullopt;
  }

  std::ifstream ifs(file_path);
  std::string access;
  std::string refresh;
  if (!std::getline(ifs, access)) {
    return std::nullopt;
  }
  if (!std::getline(ifs, refresh)) {
    return std::nullopt;
  }
  if (access.empty() || refresh.empty()) {
    return std::nullopt;
  }
  return std::make_pair(access, refresh);
}

void TokenStorage::clear() {
  const std::filesystem::path file_path(token_file_path());
  std::error_code ec;
  std::filesystem::remove(file_path, ec);
}

std::string TokenStorage::token_file_path() const {
  const std::filesystem::path file_path =
      std::filesystem::path(path_service_.config_dir()) / "tokens.txt";
  return file_path.string();
}

} // namespace stv::infra
