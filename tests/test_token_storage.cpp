#include "infra/path_service.h"
#include "infra/token_storage.h"

#include <gtest/gtest.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace {

class FakePathService : public stv::infra::PathService {
public:
  explicit FakePathService(std::string base) : base_(std::move(base)) {}

  [[nodiscard]] std::string config_dir() const override { return base_ + "/config"; }
  [[nodiscard]] std::string cache_dir() const override { return base_ + "/cache"; }
  [[nodiscard]] std::string data_dir() const override { return base_ + "/data"; }
  [[nodiscard]] std::string download_dir() const override {
    return base_ + "/downloads";
  }

private:
  std::string base_;
};

} // namespace

TEST(TokenStorageTest, SaveLoadClearRoundTrip) {
  const fs::path base = fs::temp_directory_path() / "stv_token_storage_test";
  fs::remove_all(base);

  FakePathService service(base.string());
  stv::infra::TokenStorage storage(service);

  storage.clear();
  EXPECT_FALSE(storage.load().has_value());

  storage.save("access_a", "refresh_b");
  const auto loaded = storage.load();
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->first, "access_a");
  EXPECT_EQ(loaded->second, "refresh_b");

  storage.clear();
  EXPECT_FALSE(storage.load().has_value());
  fs::remove_all(base);
}
