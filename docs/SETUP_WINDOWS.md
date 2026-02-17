# Windows 开发环境设置指南

## 前置条件

1. **Visual Studio 2019 16.11+ 或 2022**（含"使用C++的桌面开发"工作负载）
2. **CMake 3.21+**（通常随VS安装，或单独安装）
3. **Git**（用于克隆vcpkg）

## 一键设置脚本

在项目根目录运行：

```powershell
.\scripts\setup-windows.ps1
```

这将自动安装vcpkg、Ninja并配置环境变量。

---

## 手动设置步骤

### 步骤 1: 安装 vcpkg

```powershell
# 1.1 克隆vcpkg到推荐位置
git clone https://github.com/microsoft/vcpkg C:\vcpkg

# 1.2 引导vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# 1.3 验证安装
.\vcpkg.exe version
```

**预期输出**:
```
vcpkg package management program version 2024-xx-xx...
```

### 步骤 2: 安装 Ninja

```powershell
# 使用vcpkg安装Ninja（最简单）
C:\vcpkg\vcpkg.exe install ninja:x64-windows

# Ninja会安装到: C:\vcpkg\installed\x64-windows\tools\ninja\ninja.exe
```

**或者**手动下载：
1. 访问 https://github.com/ninja-build/ninja/releases
2. 下载 `ninja-win.zip`
3. 解压到 `C:\Tools\ninja\`
4. 添加到PATH（见步骤4）

### 步骤 3: 配置环境变量

**永久设置**（推荐）:

```powershell
# 设置VCPKG_ROOT
[System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\vcpkg', 'User')

# 添加vcpkg和Ninja到PATH
$currentPath = [System.Environment]::GetEnvironmentVariable('Path', 'User')
$newPath = $currentPath + ';C:\vcpkg;C:\vcpkg\installed\x64-windows\tools\ninja'
[System.Environment]::SetEnvironmentVariable('Path', $newPath, 'User')

# 重新加载环境变量（或重启终端）
$env:VCPKG_ROOT = 'C:\vcpkg'
$env:Path = [System.Environment]::GetEnvironmentVariable('Path', 'User')
```

**临时设置**（当前会话）:

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
$env:Path += ";C:\vcpkg\installed\x64-windows\tools\ninja"
```

### 步骤 4: 验证环境

```powershell
# 检查环境变量
Write-Host "VCPKG_ROOT = $env:VCPKG_ROOT"

# 检查工具可用性
ninja --version
cmake --version
cl.exe 2>&1 | Select-String "Compiler Version"
```

**预期输出**:
```
VCPKG_ROOT = C:\vcpkg
1.11.1
cmake version 3.28.x
Microsoft (R) C/C++ Optimizing Compiler Version 19.xx.xxxxx
```

如果 `cl.exe` 找不到，说明你不在Visual Studio开发环境中（见步骤5）。

### 步骤 5: 使用正确的终端

**方法 1: Developer PowerShell for VS 2022**（推荐）

1. 打开"开始"菜单
2. 搜索 `Developer PowerShell for VS 2022`
3. 以管理员身份运行
4. 导航到项目目录: `cd C:\stv-renew`

**方法 2: 手动激活开发环境**

```powershell
# 在普通PowerShell中运行
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64

# 如果是VS 2019
& "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64
```

---

## 构建项目

### 配置

```powershell
# 确保在Developer PowerShell中
cd C:\stv-renew

# 配置（仅核心库，不构建Qt应用）
cmake --preset windows-msvc

# 或配置并构建Qt应用（需要Qt 6.5.3）
cmake --preset windows-msvc-app
```

**首次运行会安装依赖**（需要5-15分钟）：
- curl[ssl]
- spdlog
- qtbase（仅-app preset）
- qtdeclarative（仅-app preset）

### 构建

```powershell
cmake --build --preset windows-msvc --parallel
```

### 测试

```powershell
ctest --preset windows-msvc
```

---

## 常见问题

### Q1: "找不到vcpkg.cmake"

**原因**: `VCPKG_ROOT` 环境变量未设置

**解决**:
```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
```

### Q2: "找不到Ninja"

**原因**: Ninja不在PATH中

**解决**:
```powershell
# 验证Ninja位置
Test-Path "C:\vcpkg\installed\x64-windows\tools\ninja\ninja.exe"

# 如果存在，添加到PATH
$env:Path += ";C:\vcpkg\installed\x64-windows\tools\ninja"
```

### Q3: "CMAKE_CXX_COMPILER not set"

**原因**: 不在Visual Studio开发环境中

**解决**: 使用 `Developer PowerShell for VS 2022` 或手动激活（见步骤5）

### Q4: vcpkg安装依赖失败

**常见原因**:
1. 网络问题（GitHub连接慢）
2. 磁盘空间不足（需要约5GB空间）

**解决**:
```powershell
# 清理缓存重试
C:\vcpkg\vcpkg.exe remove --outdated
C:\vcpkg\vcpkg.exe install curl[ssl]:x64-windows spdlog:x64-windows
```

### Q5: Qt 6.5.3找不到

**原因**: vcpkg的Qt版本可能不是6.5.3

**临时解决**（修改app/CMakeLists.txt）:
```cmake
# 从
find_package(Qt6 6.5.3 EXACT REQUIRED ...)

# 改为
find_package(Qt6 6.5 REQUIRED ...)  # 允许6.5.x
```

**或者**手动安装Qt 6.5.3:
1. 访问 https://www.qt.io/download-qt-installer
2. 安装Qt Online Installer
3. 选择Qt 6.5.3 MSVC 2019 64-bit
4. 设置环境变量:
   ```powershell
   $env:Qt6_DIR = "C:\Qt\6.5.3\msvc2019_64"
   ```

---

## 目录结构参考

```
C:\
├── vcpkg\                      # vcpkg根目录
│   ├── vcpkg.exe
│   ├── scripts\
│   │   └── buildsystems\
│   │       └── vcpkg.cmake
│   ├── installed\
│   │   └── x64-windows\
│   │       ├── bin\            # DLL运行时
│   │       ├── lib\            # 静态库
│   │       ├── include\        # 头文件
│   │       └── tools\
│   │           └── ninja\
│   └── buildtrees\             # 临时构建目录
│
└── stv-renew\                  # 项目根目录
    ├── CMakeLists.txt
    ├── CMakePresets.json
    ├── vcpkg.json
    └── build\
        └── windows-msvc\       # 构建输出
```

---

## 下一步

环境配置完成后：

1. **构建核心库**（不需要Qt）:
   ```powershell
   cmake --preset windows-msvc
   cmake --build --preset windows-msvc --parallel
   ctest --preset windows-msvc
   ```

2. **构建完整应用**（需要Qt）:
   ```powershell
   cmake --preset windows-msvc-app
   cmake --build --preset windows-msvc-app --parallel
   .\build\windows-msvc-app\app\Debug\stv_app.exe
   ```

3. **阅读平台文档**:
   - [docs/PLATFORM_WINDOWS.md](PLATFORM_WINDOWS.md)
   - [docs/PHASE1_MVP_IMPLEMENTATION_GUIDE.md](PHASE1_MVP_IMPLEMENTATION_GUIDE.md)

---

**文档版本**: v1.0
**最后更新**: 2026-02-17
**测试环境**: Windows 11 + VS 2022 + CMake 3.28
