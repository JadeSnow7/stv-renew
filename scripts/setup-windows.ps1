# Windows 环境自动设置脚本
# 功能：安装vcpkg、Ninja，配置环境变量

param(
    [string]$VcpkgRoot = "C:\vcpkg",
    [switch]$SkipVcpkg,
    [switch]$SkipNinja,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "StoryToVideo Renew - Windows 环境设置" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查管理员权限（设置环境变量需要）
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Warning "建议以管理员身份运行以永久设置环境变量"
    Write-Host "继续将只设置当前会话的环境变量" -ForegroundColor Yellow
    Write-Host ""
}

# ========================================
# 步骤 1: 检查必要工具
# ========================================
Write-Host "[1/5] 检查必要工具..." -ForegroundColor Green

# 检查Git
try {
    $gitVersion = git --version
    Write-Host "  ✓ Git 已安装: $gitVersion" -ForegroundColor Gray
} catch {
    Write-Error "Git 未安装或不在PATH中。请从 https://git-scm.com/ 安装"
    exit 1
}

# 检查CMake
try {
    $cmakeVersion = cmake --version | Select-Object -First 1
    Write-Host "  ✓ CMake 已安装: $cmakeVersion" -ForegroundColor Gray
} catch {
    Write-Error "CMake 未安装。请从 https://cmake.org/download/ 安装或通过Visual Studio安装"
    exit 1
}

# 检查MSVC编译器
try {
    $clOutput = cl.exe 2>&1 | Out-String
    if ($clOutput -match "Compiler Version (\d+\.\d+\.\d+)") {
        Write-Host "  ✓ MSVC 编译器已加载: $($matches[1])" -ForegroundColor Gray
    } else {
        throw "Cannot detect version"
    }
} catch {
    Write-Warning "MSVC 编译器未找到"
    Write-Host "  → 请在 'Developer PowerShell for VS 2022' 中运行此脚本" -ForegroundColor Yellow
    Write-Host "  → 或手动激活: " -ForegroundColor Yellow
    Write-Host '    & "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64' -ForegroundColor Cyan
    Write-Host ""
}

# ========================================
# 步骤 2: 安装/验证 vcpkg
# ========================================
Write-Host ""
Write-Host "[2/5] 设置 vcpkg..." -ForegroundColor Green

if (Test-Path "$VcpkgRoot\vcpkg.exe") {
    Write-Host "  ✓ vcpkg 已存在于 $VcpkgRoot" -ForegroundColor Gray

    if ($Force) {
        Write-Host "  ⚠ --Force 模式：跳过更新" -ForegroundColor Yellow
    } else {
        Write-Host "  → 更新 vcpkg..." -ForegroundColor Gray
        Push-Location $VcpkgRoot
        git pull
        .\bootstrap-vcpkg.bat -disableMetrics
        Pop-Location
    }
} elseif ($SkipVcpkg) {
    Write-Host "  ⊗ 跳过 vcpkg 安装（--SkipVcpkg）" -ForegroundColor Yellow
} else {
    Write-Host "  → 克隆 vcpkg 到 $VcpkgRoot..." -ForegroundColor Gray

    if (Test-Path $VcpkgRoot) {
        if ($Force) {
            Remove-Item -Recurse -Force $VcpkgRoot
        } else {
            Write-Error "目录已存在: $VcpkgRoot。使用 --Force 覆盖或选择其他目录"
            exit 1
        }
    }

    git clone https://github.com/microsoft/vcpkg $VcpkgRoot

    Write-Host "  → 引导 vcpkg..." -ForegroundColor Gray
    Push-Location $VcpkgRoot
    .\bootstrap-vcpkg.bat -disableMetrics
    Pop-Location

    Write-Host "  ✓ vcpkg 安装完成" -ForegroundColor Green
}

# 验证vcpkg
if (Test-Path "$VcpkgRoot\vcpkg.exe") {
    $vcpkgVersion = & "$VcpkgRoot\vcpkg.exe" version | Select-Object -First 1
    Write-Host "  ✓ vcpkg 可用: $vcpkgVersion" -ForegroundColor Gray
}

# ========================================
# 步骤 3: 安装 Ninja
# ========================================
Write-Host ""
Write-Host "[3/5] 设置 Ninja..." -ForegroundColor Green

$ninjaPath = "$VcpkgRoot\installed\x64-windows\tools\ninja\ninja.exe"

if (Test-Path $ninjaPath) {
    Write-Host "  ✓ Ninja 已安装于 vcpkg" -ForegroundColor Gray
} elseif ($SkipNinja) {
    Write-Host "  ⊗ 跳过 Ninja 安装（--SkipNinja）" -ForegroundColor Yellow
} else {
    Write-Host "  → 通过 vcpkg 安装 Ninja..." -ForegroundColor Gray
    & "$VcpkgRoot\vcpkg.exe" install ninja:x64-windows

    if (Test-Path $ninjaPath) {
        Write-Host "  ✓ Ninja 安装完成" -ForegroundColor Green
    } else {
        Write-Warning "Ninja 安装失败，请手动安装"
    }
}

# 验证Ninja
if (Test-Path $ninjaPath) {
    $ninjaVersion = & $ninjaPath --version
    Write-Host "  ✓ Ninja 可用: $ninjaVersion" -ForegroundColor Gray
}

# ========================================
# 步骤 4: 配置环境变量
# ========================================
Write-Host ""
Write-Host "[4/5] 配置环境变量..." -ForegroundColor Green

$currentVcpkgRoot = [System.Environment]::GetEnvironmentVariable('VCPKG_ROOT', 'User')
$currentPath = [System.Environment]::GetEnvironmentVariable('Path', 'User')

if ($currentVcpkgRoot -ne $VcpkgRoot) {
    if ($isAdmin) {
        Write-Host "  → 设置 VCPKG_ROOT = $VcpkgRoot (用户级)" -ForegroundColor Gray
        [System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', $VcpkgRoot, 'User')
    }

    Write-Host "  → 设置 VCPKG_ROOT = $VcpkgRoot (当前会话)" -ForegroundColor Gray
    $env:VCPKG_ROOT = $VcpkgRoot
}

# 添加vcpkg和Ninja到PATH
$pathsToAdd = @(
    $VcpkgRoot,
    "$VcpkgRoot\installed\x64-windows\tools\ninja"
)

$pathUpdated = $false
foreach ($pathToAdd in $pathsToAdd) {
    if ($currentPath -notlike "*$pathToAdd*") {
        if ($isAdmin) {
            Write-Host "  → 添加到 PATH: $pathToAdd (用户级)" -ForegroundColor Gray
            $currentPath += ";$pathToAdd"
            $pathUpdated = $true
        }
    }

    # 当前会话
    if ($env:Path -notlike "*$pathToAdd*") {
        $env:Path += ";$pathToAdd"
    }
}

if ($pathUpdated -and $isAdmin) {
    [System.Environment]::SetEnvironmentVariable('Path', $currentPath, 'User')
    Write-Host "  ✓ PATH 已更新（需重启终端生效）" -ForegroundColor Green
}

# ========================================
# 步骤 5: 验证环境
# ========================================
Write-Host ""
Write-Host "[5/5] 验证环境..." -ForegroundColor Green

$allGood = $true

# 检查VCPKG_ROOT
if ($env:VCPKG_ROOT) {
    Write-Host "  ✓ VCPKG_ROOT = $env:VCPKG_ROOT" -ForegroundColor Gray
} else {
    Write-Host "  ✗ VCPKG_ROOT 未设置" -ForegroundColor Red
    $allGood = $false
}

# 检查vcpkg可执行
try {
    $null = & vcpkg version 2>&1
    Write-Host "  ✓ vcpkg 在 PATH 中" -ForegroundColor Gray
} catch {
    Write-Host "  ✗ vcpkg 不在 PATH 中" -ForegroundColor Red
    $allGood = $false
}

# 检查Ninja可执行
try {
    $null = ninja --version 2>&1
    Write-Host "  ✓ ninja 在 PATH 中" -ForegroundColor Gray
} catch {
    Write-Host "  ⚠ ninja 不在 PATH 中（可能需要重启终端）" -ForegroundColor Yellow
}

# 检查CMake Preset
Write-Host ""
Write-Host "  → 测试 CMake preset..." -ForegroundColor Gray
$presetTest = cmake --list-presets 2>&1 | Out-String
if ($presetTest -match "windows-msvc") {
    Write-Host "  ✓ CMake preset 'windows-msvc' 可用" -ForegroundColor Gray
} else {
    Write-Host "  ⚠ CMake preset 不可用（可能 VCPKG_ROOT 未生效）" -ForegroundColor Yellow
}

# ========================================
# 完成
# ========================================
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
if ($allGood) {
    Write-Host "✓ 环境设置完成！" -ForegroundColor Green
} else {
    Write-Host "⚠ 环境设置完成（有警告）" -ForegroundColor Yellow
}
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 提供后续步骤
Write-Host "后续步骤:" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. 配置项目（首次运行会安装依赖，需要 5-15 分钟）:" -ForegroundColor White
Write-Host "   cmake --preset windows-msvc" -ForegroundColor Yellow
Write-Host ""
Write-Host "2. 构建:" -ForegroundColor White
Write-Host "   cmake --build --preset windows-msvc --parallel" -ForegroundColor Yellow
Write-Host ""
Write-Host "3. 测试:" -ForegroundColor White
Write-Host "   ctest --preset windows-msvc" -ForegroundColor Yellow
Write-Host ""
Write-Host "详细文档: docs/SETUP_WINDOWS.md" -ForegroundColor Gray
Write-Host ""

if (-not $isAdmin) {
    Write-Host "提示: 环境变量仅对当前会话有效。" -ForegroundColor Yellow
    Write-Host "      要永久设置，请以管理员身份重新运行此脚本。" -ForegroundColor Yellow
    Write-Host ""
}
