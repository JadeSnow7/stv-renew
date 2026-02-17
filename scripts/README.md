# Windows 构建脚本集合

## 🚀 快速开始

### 遇到构建错误？

运行快速修复脚本：

```powershell
.\scripts\quick-fix-windows.ps1
```

这将：
- ✓ 检测MSVC编译器
- ✓ 查找vcpkg安装
- ✓ 安装Ninja（如缺失）
- ✓ 设置环境变量（当前会话）

### 首次设置环境

运行完整设置脚本（需要10-20分钟）：

```powershell
# 推荐：以管理员身份运行（永久设置环境变量）
.\scripts\setup-windows.ps1

# 或指定vcpkg位置
.\scripts\setup-windows.ps1 -VcpkgRoot "D:\vcpkg"
```

这将：
- 安装vcpkg（如未安装）
- 安装Ninja
- 配置永久环境变量
- 验证所有工具

---

## 📋 脚本说明

### quick-fix-windows.ps1
**用途**: 快速解决构建错误
**时间**: 2-5分钟
**作用域**: 仅当前PowerShell会话
**需要管理员**: 否

```powershell
.\scripts\quick-fix-windows.ps1
```

### setup-windows.ps1
**用途**: 完整环境设置
**时间**: 10-20分钟（包含vcpkg安装）
**作用域**: 永久（用户级环境变量）
**需要管理员**: 建议

```powershell
# 基本用法
.\scripts\setup-windows.ps1

# 参数
.\scripts\setup-windows.ps1 -VcpkgRoot "D:\vcpkg"  # 自定义位置
.\scripts\setup-windows.ps1 -SkipVcpkg             # 跳过vcpkg（已安装）
.\scripts\setup-windows.ps1 -SkipNinja             # 跳过Ninja（已安装）
.\scripts\setup-windows.ps1 -Force                 # 强制覆盖现有安装
```

---

## ⚠️ 常见问题

### "找不到 cl.exe"

**原因**: 未在Visual Studio开发环境中运行

**解决**: 使用以下任一方法

**方法1**（推荐）:
1. 打开"开始"菜单
2. 搜索 `Developer PowerShell for VS 2022`
3. 打开该终端
4. `cd C:\stv-renew`

**方法2**: 在普通PowerShell中激活：
```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64
```

### "找不到 vcpkg.cmake"

**原因**: `VCPKG_ROOT` 环境变量未设置

**快速解决**:
```powershell
$env:VCPKG_ROOT = "C:\vcpkg"  # 临时设置
```

**永久解决**:
```powershell
[System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\vcpkg', 'User')
# 重启终端
```

### vcpkg不存在

**解决**: 安装vcpkg
```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
```

---

## 📚 详细文档

- **完整指南**: [docs/SETUP_WINDOWS.md](../docs/SETUP_WINDOWS.md)
- **平台开发**: [docs/PLATFORM_WINDOWS.md](../docs/PLATFORM_WINDOWS.md)
- **实施计划**: [docs/PHASE1_MVP_IMPLEMENTATION_GUIDE.md](../docs/PHASE1_MVP_IMPLEMENTATION_GUIDE.md)

---

## 🎯 验证环境

运行以下命令验证环境是否正确配置：

```powershell
# 1. 检查环境变量
Write-Host "VCPKG_ROOT = $env:VCPKG_ROOT"

# 2. 检查工具
cl.exe 2>&1 | Select-String "Compiler"  # MSVC
cmake --version                          # CMake
ninja --version                          # Ninja
vcpkg version                            # vcpkg

# 3. 列出CMake presets
cmake --list-presets

# 4. 测试配置（首次会安装依赖，需5-15分钟）
cmake --preset windows-msvc
```

预期输出包含：
```
VCPKG_ROOT = C:\vcpkg
Microsoft (R) C/C++ Optimizing Compiler Version 19.xx
cmake version 3.28.x
1.11.1
vcpkg package management program version 2024-xx-xx
...
"windows-msvc"
"windows-msvc-app"
```

---

## 🔄 构建流程

环境配置完成后：

```powershell
# 1. 配置（首次运行会安装curl、spdlog等，需5-15分钟）
cmake --preset windows-msvc

# 2. 构建
cmake --build --preset windows-msvc --parallel

# 3. 测试
ctest --preset windows-msvc --output-on-failure

# 4. （可选）构建Qt应用
cmake --preset windows-msvc-app
cmake --build --preset windows-msvc-app --parallel
.\build\windows-msvc-app\app\Debug\stv_app.exe
```

---

**维护者**: StoryToVideo Renew Team
**最后更新**: 2026-02-17
