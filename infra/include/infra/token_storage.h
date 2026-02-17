#pragma once

#include <optional>
#include <string>
#include <utility>

namespace stv::infra {

class PathService;

class TokenStorage {
public:
  explicit TokenStorage(const PathService &path_service);

  void save(const std::string &access_token, const std::string &refresh_token);
  [[nodiscard]] std::optional<std::pair<std::string, std::string>> load() const;
  void clear();

  [[nodiscard]] std::string token_file_path() const;

private:
  const PathService &path_service_;
};

} // namespace stv::infra
