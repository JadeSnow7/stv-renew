# Platform Guide: Linux

## Environment

- OS: Ubuntu 22.04+
- Compiler: GCC 11+ or Clang 15+
- Build tool: CMake + Ninja
- Dependencies: apt system packages

## Setup

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  libcurl4-openssl-dev \
  libspdlog-dev
```

## Build (Core + Tests)

```bash
cmake --preset linux-gcc
cmake --build --preset linux-gcc --parallel
ctest --preset linux-gcc
```

## Build App (Qt 6.5.3 required)

```bash
cmake --preset linux-gcc-app
cmake --build --preset linux-gcc-app --parallel
```

## Paths (XDG)

- Config: `${XDG_CONFIG_HOME:-~/.config}/stv_renew`
- Cache: `${XDG_CACHE_HOME:-~/.cache}/stv_renew`
- Data: `${XDG_DATA_HOME:-~/.local/share}/stv_renew`

## Phase 2 Notes

- Packaging: AppImage.
- Diagnostics: Sentry + core dump symbolization docs.
