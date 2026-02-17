# StoryToVideo Renew

Cross-platform desktop client (`Qt/QML + C++`) with backend service (`Go/Gin`).

## Platform Strategy (Phase 1)

- Priority: **Windows/macOS first**, Linux aligned.
- Toolchain:
  - Windows: MSVC + Ninja + vcpkg
  - macOS: clang + universal binary + vcpkg
  - Linux: GCC/Clang + apt packages
- Logging: `spdlog` is mandatory.
- Qt policy: **6.5.3 fixed** for app build.

## Quick Start

### 1) Client core/tests (no Qt required)

```bash
cmake --preset linux-gcc
cmake --build --preset linux-gcc --parallel
ctest --preset linux-gcc
```

On Windows/macOS:

```bash
cmake --preset windows-msvc
cmake --build --preset windows-msvc --parallel
ctest --preset windows-msvc
```

```bash
cmake --preset macos-universal
cmake --build --preset macos-universal --parallel
ctest --preset macos-universal
```

### 2) Client app build (Qt 6.5.3)

```bash
cmake --preset windows-msvc-app
cmake --build --preset windows-msvc-app --parallel
```

```bash
cmake --preset macos-universal-app
cmake --build --preset macos-universal-app --parallel
```

```bash
cmake --preset linux-gcc-app
cmake --build --preset linux-gcc-app --parallel
```

### 3) Server

```bash
cd server
go run ./cmd/stv-server
```

Demo account:

- `demo@stv.local`
- `demo123456`

## Key Docs

- `docs/CROSS_PLATFORM_COMPAT_PLAN.md`
- `docs/PLATFORM_WINDOWS.md`
- `docs/PLATFORM_MACOS.md`
- `docs/PLATFORM_LINUX.md`
- `docs/CI_MATRIX.md`
- `docs/RELEASE_SIGNING_NOTARIZATION.md`
