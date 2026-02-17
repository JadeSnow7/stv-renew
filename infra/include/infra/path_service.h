#pragma once

#include <memory>
#include <string>

namespace stv::infra {

class PathService {
public:
  virtual ~PathService() = default;

  [[nodiscard]] virtual std::string config_dir() const = 0;
  [[nodiscard]] virtual std::string cache_dir() const = 0;
  [[nodiscard]] virtual std::string data_dir() const = 0;
  [[nodiscard]] virtual std::string download_dir() const = 0;

  static std::unique_ptr<PathService> create();
};

} // namespace stv::infra
