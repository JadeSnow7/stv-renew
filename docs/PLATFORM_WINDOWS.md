# Platform Guide: Windows

## Environment

- OS: Windows 10/11
- Compiler: MSVC (VS 2022 recommended)
- Build tool: CMake + Ninja
- Package manager: vcpkg (manifest mode)

## Setup

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
$env:VCPKG_ROOT = "C:\vcpkg"
```

## Build (Core + Tests)

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc --parallel
ctest --preset windows-msvc
```

## Build App (Qt 6.5.3)

```powershell
cmake --preset windows-msvc-app
cmake --build --preset windows-msvc-app --parallel
```

## Paths

- Config: `%APPDATA%\stv_renew`
- Cache: `%LOCALAPPDATA%\stv_renew\cache`
- Data: `%LOCALAPPDATA%\stv_renew\data`

## Phase 2 Notes

- Packaging: MSI via CPack/WiX.
- Signing: `signtool`.
- Diagnostics: Sentry + PDB symbol upload.
