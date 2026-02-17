# 快速修复 - 在当前 PowerShell 执行

# 1. 检查是否在 Visual Studio 开发环境中
$clExists = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $clExists) {
    Write-Host "错误: 未检测到 MSVC 编译器" -ForegroundColor Red
    Write-Host ""
    Write-Host "请关闭当前窗口，按以下步骤操作:" -ForegroundColor Yellow
    Write-Host "1. 打开'开始'菜单" -ForegroundColor White
    Write-Host "2. 搜索 'Developer PowerShell for VS 2022'" -ForegroundColor White
    Write-Host "3. 打开该终端" -ForegroundColor White
    Write-Host "4. cd C:\stv-renew" -ForegroundColor White
    Write-Host "5. 重新运行此脚本" -ForegroundColor White
    Write-Host ""
    pause
    exit 1
}

Write-Host "✓ 检测到 MSVC 编译器" -ForegroundColor Green

# 2. 设置 VCPKG_ROOT（假设安装在 C:\vcpkg）
$vcpkgLocations = @("C:\vcpkg", "C:\Tools\vcpkg", "$env:USERPROFILE\vcpkg")
$vcpkgFound = $null

foreach ($loc in $vcpkgLocations) {
    if (Test-Path "$loc\vcpkg.exe") {
        $vcpkgFound = $loc
        break
    }
}

if ($vcpkgFound) {
    $env:VCPKG_ROOT = $vcpkgFound
    Write-Host "✓ 找到 vcpkg: $vcpkgFound" -ForegroundColor Green
} else {
    Write-Host "✗ vcpkg 未安装" -ForegroundColor Red
    Write-Host ""
    Write-Host "运行自动安装脚本:" -ForegroundColor Yellow
    Write-Host "  .\scripts\setup-windows.ps1" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "或手动安装:" -ForegroundColor Yellow
    Write-Host "  git clone https://github.com/microsoft/vcpkg C:\vcpkg" -ForegroundColor Cyan
    Write-Host "  C:\vcpkg\bootstrap-vcpkg.bat" -ForegroundColor Cyan
    Write-Host ""
    pause
    exit 1
}

# 3. 安装 Ninja（如果缺失）
$ninjaPath = "$vcpkgFound\installed\x64-windows\tools\ninja\ninja.exe"
if (-not (Test-Path $ninjaPath)) {
    Write-Host "→ 安装 Ninja（首次需要2-3分钟）..." -ForegroundColor Yellow
    & "$vcpkgFound\vcpkg.exe" install ninja:x64-windows
}

$env:Path += ";$vcpkgFound\installed\x64-windows\tools\ninja"

# 验证
try {
    $ninjaVersion = ninja --version
    Write-Host "✓ Ninja 版本: $ninjaVersion" -ForegroundColor Green
} catch {
    Write-Host "✗ Ninja 安装失败" -ForegroundColor Red
    exit 1
}

# 4. 显示当前环境
Write-Host ""
Write-Host "========== 当前环境 ==========" -ForegroundColor Cyan
Write-Host "VCPKG_ROOT = $env:VCPKG_ROOT"
Write-Host "工具链文件 = $env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
Write-Host ""

# 5. 测试配置
Write-Host "→ 测试 CMake 配置..." -ForegroundColor Yellow
cmake --list-presets

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "环境已就绪！现在可以运行:" -ForegroundColor Green
Write-Host "  cmake --preset windows-msvc" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Green
