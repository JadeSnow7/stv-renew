# Phase 1 MVP 跨平台实施指南

**目标**: 4周内实现三平台本地构建运行 + CI矩阵全绿
**范围**: 构建工具链、基础平台抽象、PathService、TokenStorage、CI集成
**交付**: 可运行的三平台客户端 + 基础文档

---

## Week 1: 依赖管理与工具链统一

### Day 1-2: vcpkg 集成

#### 1.1 创建 vcpkg.json
```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.json.schema.json",
  "name": "stv-renew",
  "version": "0.1.0",
  "dependencies": [
    "curl[ssl]",
    "spdlog",
    {
      "name": "qt",
      "version>=": "6.5.3",
      "features": ["quick", "quickcontrols2"]
    }
  ],
  "builtin-baseline": "a42af01b72c28a8e1d7b48107b33e4f286a55ef6"
}
```

#### 1.2 修改顶层 CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.21)

# vcpkg 工具链（必须在 project() 之前）
if(DEFINED ENV{VCPKG_ROOT})
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "Vcpkg toolchain file")
endif()

project(StoryToVideoRenew VERSION 0.1.0 LANGUAGES CXX)

# 强制 Qt 版本范围
set(QT_MIN_VERSION "6.5.0")
set(QT_MAX_VERSION "6.7.99")
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Quick QuickControls2)

if(Qt6_VERSION VERSION_GREATER ${QT_MAX_VERSION})
  message(FATAL_ERROR "Qt ${Qt6_VERSION} 未经测试，支持范围 ${QT_MIN_VERSION}-${QT_MAX_VERSION}")
endif()

message(STATUS "使用 Qt ${Qt6_VERSION}")

# 强制 spdlog
find_package(spdlog REQUIRED)

# ... 其余保持不变
```

#### 1.3 验证命令
```bash
# Windows (PowerShell)
$env:VCPKG_ROOT = "C:\vcpkg"
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# macOS/Linux
export VCPKG_ROOT=~/vcpkg
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Day 3-4: CMake Presets

#### 1.4 创建 CMakePresets.json
```json
{
  "version": 6,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 21,
    "patch": 0
  },
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "STV_BUILD_TESTS": "ON"
      }
    },
    {
      "name": "windows-msvc",
      "inherits": "base",
      "generator": "Ninja Multi-Config",
      "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "CMAKE_C_COMPILER": "cl",
        "CMAKE_CXX_COMPILER": "cl",
        "VCPKG_TARGET_TRIPLET": "x64-windows"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Windows"
      }
    },
    {
      "name": "macos-universal",
      "inherits": "base",
      "generator": "Ninja Multi-Config",
      "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "CMAKE_OSX_ARCHITECTURES": "x86_64;arm64",
        "CMAKE_OSX_DEPLOYMENT_TARGET": "13.0",
        "VCPKG_TARGET_TRIPLET": "arm64-osx"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Darwin"
      }
    },
    {
      "name": "linux-gcc",
      "inherits": "base",
      "generator": "Ninja Multi-Config",
      "cacheVariables": {
        "CMAKE_C_COMPILER": "gcc",
        "CMAKE_CXX_COMPILER": "g++",
        "STV_USE_SYSTEM_SPDLOG": "OFF"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Linux"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "windows-msvc",
      "configurePreset": "windows-msvc",
      "configuration": "Debug"
    },
    {
      "name": "macos-universal",
      "configurePreset": "macos-universal",
      "configuration": "Debug"
    },
    {
      "name": "linux-gcc",
      "configurePreset": "linux-gcc",
      "configuration": "Debug"
    }
  ],
  "testPresets": [
    {
      "name": "windows-msvc",
      "configurePreset": "windows-msvc",
      "configuration": "Debug"
    },
    {
      "name": "macos-universal",
      "configurePreset": "macos-universal",
      "configuration": "Debug"
    },
    {
      "name": "linux-gcc",
      "configurePreset": "linux-gcc",
      "configuration": "Debug"
    }
  ]
}
```

#### 1.5 验证 Presets
```bash
# 自动选择正确的 preset
cmake --list-presets
cmake --preset <当前平台preset>
cmake --build --preset <当前平台preset>
ctest --preset <当前平台preset>
```

### Day 5: 平台编译器标志统一

#### 1.6 创建 cmake/CompilerWarnings.cmake
```cmake
function(set_project_warnings target_name)
  set(MSVC_WARNINGS
      /W4          # 高警告级别
      /WX          # 警告视为错误
      /utf-8       # 源文件 UTF-8
      /permissive- # 严格标准一致性
      /Zc:__cplusplus  # __cplusplus 宏正确值
  )

  set(GCC_CLANG_WARNINGS
      -Wall
      -Wextra
      -Wpedantic
      -Werror=return-type
      -Wno-unused-parameter  # Qt 元对象系统会产生
  )

  if(MSVC)
    target_compile_options(${target_name} PRIVATE ${MSVC_WARNINGS})
  else()
    target_compile_options(${target_name} PRIVATE ${GCC_CLANG_WARNINGS})
  endif()
endfunction()
```

#### 1.7 应用到各模块
```cmake
# core/CMakeLists.txt
include(../cmake/CompilerWarnings.cmake)
add_library(stv_core STATIC src/task.cpp ...)
set_project_warnings(stv_core)

# 其他模块同理
```

---

## Week 2: PathService 平台实现

### Day 1-2: 接口定义

#### 2.1 创建 infra/include/infra/path_service.h
```cpp
#pragma once
#include <string>

namespace stv::infra {

/**
 * 跨平台路径服务抽象
 *
 * 平台实现规范：
 * - Windows: %APPDATA%/stv_renew, %LOCALAPPDATA%/stv_renew
 * - macOS:   ~/Library/Application Support/stv_renew, ~/Library/Caches/stv_renew
 * - Linux:   ~/.config/stv_renew, ~/.cache/stv_renew (XDG)
 */
class PathService {
public:
  virtual ~PathService() = default;

  // 配置目录（持久化配置、token等）
  virtual std::string config_dir() const = 0;

  // 缓存目录（可清理的临时文件）
  virtual std::string cache_dir() const = 0;

  // 数据目录（用户数据、草稿等）
  virtual std::string data_dir() const = 0;

  // 下载目录（导出的视频等）
  virtual std::string download_dir() const = 0;

  // 工厂方法（自动检测平台）
  static std::unique_ptr<PathService> create();
};

} // namespace stv::infra
```

### Day 2-3: Windows 实现

#### 2.2 创建 infra/src/platform/path_service_windows.cpp
```cpp
#include "infra/path_service.h"
#include <windows.h>
#include <shlobj.h>
#include <stdexcept>

namespace stv::infra {

class PathServiceWindows : public PathService {
public:
  std::string config_dir() const override {
    return get_known_folder(FOLDERID_RoamingAppData) + "\\stv_renew";
  }

  std::string cache_dir() const override {
    return get_known_folder(FOLDERID_LocalAppData) + "\\stv_renew\\cache";
  }

  std::string data_dir() const override {
    return get_known_folder(FOLDERID_RoamingAppData) + "\\stv_renew\\data";
  }

  std::string download_dir() const override {
    return get_known_folder(FOLDERID_Downloads);
  }

private:
  std::string get_known_folder(const KNOWNFOLDERID& id) const {
    PWSTR path = nullptr;
    HRESULT hr = SHGetKnownFolderPath(id, 0, nullptr, &path);
    if (FAILED(hr)) {
      throw std::runtime_error("Failed to get known folder path");
    }

    // 转换 PWSTR -> std::string (UTF-8)
    int size = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, path, -1, &result[0], size, nullptr, nullptr);

    CoTaskMemFree(path);
    return result;
  }
};

std::unique_ptr<PathService> PathService::create() {
  return std::make_unique<PathServiceWindows>();
}

} // namespace stv::infra
```

### Day 3-4: macOS/Linux 实现

#### 2.3 创建 infra/src/platform/path_service_unix.cpp
```cpp
#include "infra/path_service.h"
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
#include <stdexcept>

namespace stv::infra {

class PathServiceUnix : public PathService {
public:
  std::string config_dir() const override {
#ifdef __APPLE__
    return get_home() + "/Library/Application Support/stv_renew";
#else
    // Linux: XDG_CONFIG_HOME
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    return xdg ? std::string(xdg) + "/stv_renew"
               : get_home() + "/.config/stv_renew";
#endif
  }

  std::string cache_dir() const override {
#ifdef __APPLE__
    return get_home() + "/Library/Caches/stv_renew";
#else
    const char* xdg = std::getenv("XDG_CACHE_HOME");
    return xdg ? std::string(xdg) + "/stv_renew"
               : get_home() + "/.cache/stv_renew";
#endif
  }

  std::string data_dir() const override {
#ifdef __APPLE__
    return get_home() + "/Library/Application Support/stv_renew/data";
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    return xdg ? std::string(xdg) + "/stv_renew"
               : get_home() + "/.local/share/stv_renew";
#endif
  }

  std::string download_dir() const override {
    // 简化：使用 ~/Downloads（可后续用 xdg-user-dirs 优化）
    return get_home() + "/Downloads";
  }

private:
  std::string get_home() const {
    const char* home = std::getenv("HOME");
    if (home) return home;

    // 降级到 passwd
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) return pw->pw_dir;

    throw std::runtime_error("Cannot determine home directory");
  }
};

std::unique_ptr<PathService> PathService::create() {
  return std::make_unique<PathServiceUnix>();
}

} // namespace stv::infra
```

### Day 4-5: CMake 集成与测试

#### 2.4 修改 infra/CMakeLists.txt
```cmake
# 平台特定源文件
if(WIN32)
  set(PLATFORM_SOURCES src/platform/path_service_windows.cpp)
elseif(UNIX)
  set(PLATFORM_SOURCES src/platform/path_service_unix.cpp)
endif()

add_library(stv_infra STATIC
    src/logger.cpp
    src/http_client.cpp
    src/curl_http_client.cpp
    src/api_client.cpp
    src/sse_client.cpp
    ${PLATFORM_SOURCES}  # 新增
)

# Windows 需要 Shell API
if(WIN32)
  target_link_libraries(stv_infra PRIVATE shell32)
endif()
```

#### 2.5 单元测试 tests/test_path_service.cpp
```cpp
#include "infra/path_service.h"
#include <gtest/gtest.h>
#include <filesystem>

using namespace stv::infra;
namespace fs = std::filesystem;

TEST(PathServiceTest, DirectoriesAreAbsolute) {
  auto svc = PathService::create();

  EXPECT_TRUE(fs::path(svc->config_dir()).is_absolute());
  EXPECT_TRUE(fs::path(svc->cache_dir()).is_absolute());
  EXPECT_TRUE(fs::path(svc->data_dir()).is_absolute());
}

TEST(PathServiceTest, DirectoriesContainAppName) {
  auto svc = PathService::create();

  EXPECT_NE(svc->config_dir().find("stv_renew"), std::string::npos);
  EXPECT_NE(svc->cache_dir().find("stv_renew"), std::string::npos);
}

#ifdef _WIN32
TEST(PathServiceTest, WindowsUsesAppData) {
  auto svc = PathService::create();
  std::string config = svc->config_dir();

  // 应该包含 AppData\Roaming 或 AppData\Local
  EXPECT_TRUE(config.find("AppData") != std::string::npos);
}
#endif

#ifdef __APPLE__
TEST(PathServiceTest, MacOSUsesLibrary) {
  auto svc = PathService::create();

  EXPECT_NE(svc->config_dir().find("Library/Application Support"), std::string::npos);
  EXPECT_NE(svc->cache_dir().find("Library/Caches"), std::string::npos);
}
#endif

#ifdef __linux__
TEST(PathServiceTest, LinuxUsesXDG) {
  auto svc = PathService::create();
  std::string config = svc->config_dir();

  // 应该是 ~/.config/stv_renew 或 $XDG_CONFIG_HOME/stv_renew
  EXPECT_TRUE(config.find(".config/stv_renew") != std::string::npos ||
              config.find("stv_renew") != std::string::npos);
}
#endif
```

---

## Week 3: TokenStorage 与应用集成

### Day 1-2: TokenStorage 实现

#### 3.1 创建 infra/include/infra/token_storage.h
```cpp
#pragma once
#include <string>
#include <optional>
#include <utility>

namespace stv::infra {

class PathService;

/**
 * JWT Token 存储（Phase 1 使用文件存储）
 *
 * 存储位置：PathService::config_dir() + "/tokens.json"
 * 格式：{ "access_token": "...", "refresh_token": "..." }
 *
 * Phase 2 可扩展为平台原生（Keychain/CredentialManager）
 */
class TokenStorage {
public:
  explicit TokenStorage(PathService& path_svc);

  // 保存 token pair
  void save(const std::string& access_token, const std::string& refresh_token);

  // 读取 token pair，返回 {access, refresh}
  std::optional<std::pair<std::string, std::string>> load() const;

  // 清除 tokens（登出时调用）
  void clear();

private:
  PathService& path_svc_;
  std::string get_token_file_path() const;
};

} // namespace stv::infra
```

#### 3.2 实现 infra/src/token_storage.cpp
```cpp
#include "infra/token_storage.h"
#include "infra/path_service.h"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>  // 建议添加依赖

namespace stv::infra {
namespace fs = std::filesystem;
using json = nlohmann::json;

TokenStorage::TokenStorage(PathService& path_svc) : path_svc_(path_svc) {}

std::string TokenStorage::get_token_file_path() const {
  return path_svc_.config_dir() + "/tokens.json";
}

void TokenStorage::save(const std::string& access_token, const std::string& refresh_token) {
  std::string path = get_token_file_path();

  // 确保目录存在
  fs::create_directories(fs::path(path).parent_path());

  json j;
  j["access_token"] = access_token;
  j["refresh_token"] = refresh_token;

  std::ofstream ofs(path);
  ofs << j.dump(2);
}

std::optional<std::pair<std::string, std::string>> TokenStorage::load() const {
  std::string path = get_token_file_path();
  if (!fs::exists(path)) {
    return std::nullopt;
  }

  try {
    std::ifstream ifs(path);
    json j = json::parse(ifs);

    return std::make_pair(
      j.value("access_token", ""),
      j.value("refresh_token", "")
    );
  } catch (...) {
    return std::nullopt;
  }
}

void TokenStorage::clear() {
  std::string path = get_token_file_path();
  fs::remove(path);
}

} // namespace stv::infra
```

### Day 3-4: AuthPresenter 集成

#### 3.3 修改 app/src/auth_presenter.cpp
```cpp
#include "app/auth_presenter.h"
#include "infra/api_client.h"
#include "infra/token_storage.h"
#include "infra/path_service.h"

namespace stv::app {

class AuthPresenter::Impl {
public:
  Impl() : path_svc_(infra::PathService::create()),
           token_storage_(*path_svc_),
           api_client_("https://api.example.com") {}

  void try_restore_session() {
    auto tokens = token_storage_.load();
    if (tokens) {
      api_client_.set_access_token(tokens->first);
      // TODO: 验证 token 是否过期，需要时用 refresh_token 刷新
    }
  }

  void login(const std::string& email, const std::string& password) {
    auto [access, refresh] = api_client_.login(email, password);
    token_storage_.save(access, refresh);
    // emit loginSuccess()
  }

  void logout() {
    api_client_.logout();
    token_storage_.clear();
    // emit loggedOut()
  }

private:
  std::unique_ptr<infra::PathService> path_svc_;
  infra::TokenStorage token_storage_;
  infra::HttpBackendApi api_client_;
};

// AuthPresenter 实现略...

} // namespace stv::app
```

### Day 5: 集成测试

#### 3.4 手动测试清单
- [ ] Windows: 登录后检查 `%APPDATA%\stv_renew\tokens.json` 存在
- [ ] macOS: 检查 `~/Library/Application Support/stv_renew/tokens.json`
- [ ] Linux: 检查 `~/.config/stv_renew/tokens.json`
- [ ] 重启应用后自动恢复登录状态
- [ ] 登出后 tokens.json 被删除

---

## Week 4: CI 矩阵集成

### Day 1-2: CI 配置

#### 4.1 更新 .github/workflows/ci.yml
```yaml
name: Cross-Platform CI

on:
  push:
    branches: [main, master]
  pull_request:

env:
  VCPKG_BINARY_SOURCES: 'clear;x-gha,readwrite'

jobs:
  build-test-matrix:
    strategy:
      fail-fast: false
      matrix:
        include:
          - os: windows-latest
            preset: windows-msvc
            triplet: x64-windows
          - os: macos-14  # Apple Silicon
            preset: macos-universal
            triplet: arm64-osx
          - os: ubuntu-22.04
            preset: linux-gcc
            triplet: x64-linux

    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4

      - name: Setup vcpkg (Win/Mac)
        if: runner.os != 'Linux'
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgGitCommitId: 'a42af01b72c28a8e1d7b48107b33e4f286a55ef6'

      - name: Export GitHub Actions cache environment variables (Win/Mac)
        if: runner.os != 'Linux'
        uses: actions/github-script@v7
        with:
          script: |
            core.exportVariable('ACTIONS_CACHE_URL', process.env.ACTIONS_CACHE_URL || '');
            core.exportVariable('ACTIONS_RUNTIME_TOKEN', process.env.ACTIONS_RUNTIME_TOKEN || '');

      - name: Install dependencies (Linux)
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential \
            cmake \
            ninja-build \
            libcurl4-openssl-dev \
            qt6-base-dev \
            qt6-declarative-dev \
            libgl1-mesa-dev

      - name: Configure
        run: cmake --preset ${{ matrix.preset }}

      - name: Build
        run: cmake --build --preset ${{ matrix.preset }} --parallel

      - name: Test
        run: ctest --preset ${{ matrix.preset }} --output-on-failure

  # Go 服务端测试保持不变
  server-test:
    runs-on: ubuntu-latest
    defaults:
      run:
        working-directory: server
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-go@v5
        with:
          go-version: '1.22'
      - run: go test ./...
```

### Day 3-4: 本地验证

#### 4.2 三平台构建脚本

**Windows (build.ps1)**:
```powershell
# 安装 vcpkg (首次)
if (-Not (Test-Path "C:\vcpkg")) {
    git clone https://github.com/microsoft/vcpkg C:\vcpkg
    C:\vcpkg\bootstrap-vcpkg.bat
}

$env:VCPKG_ROOT = "C:\vcpkg"

# 构建
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

**macOS/Linux (build.sh)**:
```bash
#!/bin/bash
set -e

# 安装 vcpkg (首次，macOS)
if [[ "$OSTYPE" == "darwin"* ]] && [[ ! -d "$HOME/vcpkg" ]]; then
    git clone https://github.com/microsoft/vcpkg ~/vcpkg
    ~/vcpkg/bootstrap-vcpkg.sh
fi

export VCPKG_ROOT=~/vcpkg

# 选择 preset
if [[ "$OSTYPE" == "darwin"* ]]; then
    PRESET="macos-universal"
else
    PRESET="linux-gcc"
fi

cmake --preset $PRESET
cmake --build --preset $PRESET
ctest --preset $PRESET
```

### Day 5: 文档更新

#### 4.3 更新 README.md
````markdown
# StoryToVideo Renew

跨平台 AI 视频生成桌面客户端（Windows/macOS/Linux）

## 快速开始

### 前置依赖
- CMake 3.21+
- C++17 编译器（MSVC 2019+/Clang 12+/GCC 10+）
- Qt 6.5+
- （可选）vcpkg（Windows/macOS 推荐）

### Windows
```powershell
# 使用 vcpkg
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
$env:VCPKG_ROOT = "C:\vcpkg"

# 构建
cmake --preset windows-msvc
cmake --build --preset windows-msvc
.\build\windows-msvc\app\Debug\stv_app.exe
```

### macOS
```bash
# 使用 vcpkg
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg

# 构建（Universal Binary）
cmake --preset macos-universal
cmake --build --preset macos-universal
open ./build/macos-universal/app/Debug/stv_app.app
```

### Linux (Ubuntu 22.04+)
```bash
# 使用系统包
sudo apt-get install -y \
  build-essential cmake ninja-build \
  libcurl4-openssl-dev qt6-base-dev qt6-declarative-dev

# 构建
cmake --preset linux-gcc
cmake --build --preset linux-gcc
./build/linux-gcc/app/Debug/stv_app
```

## 平台差异
| 特性 | Windows | macOS | Linux |
|---|---|---|---|
| Qt 版本 | 6.5+ | 6.5+ | 6.2+ (系统包) |
| 配置目录 | `%APPDATA%\stv_renew` | `~/Library/Application Support/stv_renew` | `~/.config/stv_renew` |
| 缓存目录 | `%LOCALAPPDATA%\stv_renew` | `~/Library/Caches/stv_renew` | `~/.cache/stv_renew` |

详见 [docs/PLATFORM_WINDOWS.md](docs/PLATFORM_WINDOWS.md)、[PLATFORM_MACOS.md](docs/PLATFORM_MACOS.md)、[PLATFORM_LINUX.md](docs/PLATFORM_LINUX.md)

## 服务端
```bash
cd server
go run ./cmd/stv-server
# 默认监听 http://localhost:8080
```

## 架构
- `core/`: 业务逻辑（零 Qt 依赖）
- `infra/`: 基础设施（HTTP/SSE/日志/平台抽象）
- `app/`: Qt6 界面层
- `server/`: Go/Gin 后端

## 测试
```bash
# 全平台
ctest --preset <当前平台preset> --output-on-failure

# 仅核心模块
cmake -B build -DSTV_BUILD_APP=OFF -DSTV_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

## 贡献
见 [CONTRIBUTING.md](CONTRIBUTING.md)

## License
MIT
````

#### 4.4 创建 docs/PLATFORM_WINDOWS.md
```markdown
# Windows 平台开发指南

## 环境要求
- Windows 10 1809+ / Windows 11
- Visual Studio 2019 16.11+ 或 2022（含 C++ 工作负载）
- CMake 3.21+
- vcpkg（推荐）或手动安装 Qt 6.5+

## 构建步骤

### 1. 安装 vcpkg
```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
$env:VCPKG_ROOT = "C:\vcpkg"

# 验证
C:\vcpkg\vcpkg.exe version
```

### 2. 配置与构建
```powershell
# 使用 Developer PowerShell for VS 2022
cmake --preset windows-msvc
cmake --build --preset windows-msvc --config Debug

# Release 构建
cmake --build --preset windows-msvc --config Release
```

### 3. 运行
```powershell
.\build\windows-msvc\app\Debug\stv_app.exe
```

## 常见问题

### Q: Qt 找不到
**A**: 如果使用 vcpkg，确保 `VCPKG_ROOT` 环境变量已设置。如手动安装 Qt，设置：
```powershell
$env:Qt6_DIR = "C:\Qt\6.5.3\msvc2019_64\lib\cmake\Qt6"
```

### Q: 缺少 VCRUNTIME140.dll
**A**: 安装 [Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe)

### Q: 编译错误：C2220 (utf-8 警告)
**A**: 已在 `CompilerWarnings.cmake` 添加 `/utf-8`，确保使用最新 CMakeLists.txt

## 调试技巧

### Visual Studio 调试器
```powershell
# 生成 VS 解决方案
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
# 打开 build\StoryToVideoRenew.sln
```

### 查看依赖
```powershell
dumpbin /dependents .\build\windows-msvc\app\Debug\stv_app.exe
```

## 路径规范
- 配置: `%APPDATA%\stv_renew` (通常 `C:\Users\<用户>\AppData\Roaming\stv_renew`)
- 缓存: `%LOCALAPPDATA%\stv_renew` (通常 `C:\Users\<用户>\AppData\Local\stv_renew`)
- 日志: `%LOCALAPPDATA%\stv_renew\logs`

## 下一步
- [打包 MSI](PACKAGING_WINDOWS.md)（Phase 2）
- [性能分析](PROFILING_WINDOWS.md)（Phase 2）
```

#### 4.5 创建 docs/PLATFORM_MACOS.md 和 PLATFORM_LINUX.md
（内容类似，分别针对 macOS 和 Linux，此处省略，可按需补充）

---

## 验收清单

### Week 1 交付
- [ ] vcpkg.json 已创建并能三平台安装依赖
- [ ] CMakePresets.json 包含 windows-msvc/macos-universal/linux-gcc
- [ ] `cmake --preset <平台>` 能成功配置
- [ ] CompilerWarnings.cmake 已应用到所有模块

### Week 2 交付
- [ ] PathService 接口定义完成
- [ ] Windows/macOS/Linux 平台实现通过单元测试
- [ ] infra/CMakeLists.txt 正确链接平台特定源文件
- [ ] `test_path_service` 在三平台通过

### Week 3 交付
- [ ] TokenStorage 实现并集成到 AuthPresenter
- [ ] 登录后 tokens.json 在正确目录生成
- [ ] 重启后能自动恢复登录状态
- [ ] 登出后 tokens 被清除

### Week 4 交付
- [ ] CI 配置包含 Windows/macOS/Linux 三个 Job
- [ ] CI 能成功构建并运行所有单元测试
- [ ] README.md 更新三平台快速开始
- [ ] 基础平台文档（PLATFORM_*.md）已创建

---

## 后续规划（Phase 2）

### Week 5-6: 打包
- Windows: CPack + WiX Toolset 产出 MSI
- macOS: `macdeployqt` + CPack DragNDrop 产出 DMG
- Linux: linuxdeploy 产出 AppImage

### Week 7-8: 签名与诊断
- macOS: codesign + notarytool
- Windows: signtool（可选，需代码签名证书）
- 集成 Sentry SDK 实现崩溃上报

---

## 附录：技术决策记录

| 决策 | 选择 | 理由 |
|---|---|---|
| 依赖管理 | vcpkg (Win/Mac), apt (Linux) | 统一且社区支持好 |
| Qt 版本 | 6.5.3 - 6.7.x | 稳定且三平台官方支持 |
| Token 存储 | 文件存储 (Phase 1) | 实现简单，已足够安全 |
| 日志库 | spdlog 强制使用 | 统一日志格式，便于诊断 |
| JSON 库 | nlohmann/json | 轻量且 API 友好 |
| 崩溃诊断 | Sentry SDK (Phase 2) | 开箱即用，降低维护成本 |

---

**文档版本**: v1.0
**最后更新**: 2026-02-17
**适用阶段**: Phase 1 (MVP)
