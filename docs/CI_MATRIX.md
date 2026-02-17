# CI Matrix

## Jobs

### `build-test-matrix`

- OS matrix:
  - `windows-latest` -> preset `windows-msvc`
  - `macos-14` -> preset `macos-universal`
  - `ubuntu-22.04` -> preset `linux-gcc`
- Scope: configure + build + ctest (core/infra/tests).
- Dependency policy:
  - Windows/macOS: vcpkg setup with pinned commit.
  - Linux: apt install (`curl`, `spdlog`, toolchain).

### `server-go-matrix`

- OS matrix:
  - `ubuntu-latest`
  - `windows-latest`
  - `macos-14`
- Scope: `go mod download` + `go test ./...`.
- Purpose: ensure backend developer workflow works on all three desktop OSes.

## Cache Strategy

1. vcpkg binary cache is handled by `run-vcpkg` action.
2. CMake build directories are preset-scoped and isolated per OS.
3. No cross-OS cache sharing.

## Failure Triage

1. Configure failure: verify preset + toolchain + dependency resolution.
2. Build failure: check compiler-specific diagnostics (MSVC vs clang vs GCC).
3. Test failure: reproduce locally with same preset.
