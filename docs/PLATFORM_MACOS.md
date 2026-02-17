# Platform Guide: macOS

## Environment

- OS: macOS 13+
- Compiler: Apple clang (Xcode 15+)
- Build tool: CMake + Ninja
- Package manager: vcpkg (manifest mode)

## Setup

```bash
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg
```

## Build (Core + Tests)

```bash
cmake --preset macos-universal
cmake --build --preset macos-universal --parallel
ctest --preset macos-universal
```

## Build App (Qt 6.5.3)

```bash
cmake --preset macos-universal-app
cmake --build --preset macos-universal-app --parallel
```

## Paths

- Config: `~/Library/Application Support/stv_renew`
- Cache: `~/Library/Caches/stv_renew`
- Data: `~/Library/Application Support/stv_renew/data`

## Phase 2 Notes

- Packaging: `.app` + DMG.
- Signing/notarization: `codesign -> notarytool -> stapler`.
- Diagnostics: Sentry + dSYM symbol upload.
