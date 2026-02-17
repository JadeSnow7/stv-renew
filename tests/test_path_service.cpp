#include "infra/path_service.h"

#include <gtest/gtest.h>

#include <filesystem>

namespace fs = std::filesystem;

TEST(PathServiceTest, PathsAreAbsoluteAndNonEmpty) {
  auto service = stv::infra::PathService::create();

  const fs::path config(service->config_dir());
  const fs::path cache(service->cache_dir());
  const fs::path data(service->data_dir());
  const fs::path downloads(service->download_dir());

  EXPECT_FALSE(config.empty());
  EXPECT_FALSE(cache.empty());
  EXPECT_FALSE(data.empty());
  EXPECT_FALSE(downloads.empty());

  EXPECT_TRUE(config.is_absolute());
  EXPECT_TRUE(cache.is_absolute());
  EXPECT_TRUE(data.is_absolute());
  EXPECT_TRUE(downloads.is_absolute());
}

TEST(PathServiceTest, AppScopedDirectoriesContainProjectName) {
  auto service = stv::infra::PathService::create();

  EXPECT_NE(service->config_dir().find("stv_renew"), std::string::npos);
  EXPECT_NE(service->cache_dir().find("stv_renew"), std::string::npos);
}
