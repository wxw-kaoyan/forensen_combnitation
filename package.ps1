# TASoftware 打包脚本
# 用法: cd 到 TASoftware 目录后运行  .\package.ps1
# 首次需执行: Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$ProjectDir = $PSScriptRoot
$BuildDir   = Join-Path $ProjectDir "build\release"
$PackageDir = Join-Path $ProjectDir "package"

$QtDir     = "B:/qt/6.8.3/mingw_64"
$CmakeExe  = "B:/qt/Tools/CMake_64/bin/cmake.exe"
$MakeExe   = "B:/qt/Tools/mingw1310_64/bin/mingw32-make.exe"
$MingwBin  = "B:/qt/Tools/mingw1310_64/bin"
$WinDeploy = "B:/qt/6.8.3/mingw_64/bin/windeployqt.exe"

$env:PATH = "B:/qt/Tools/CMake_64/bin;B:/qt/Tools/mingw1310_64/bin;B:/qt/6.8.3/mingw_64/bin;$env:PATH"

Write-Host "=== 1. 配置 CMake (Release) ===" -ForegroundColor Cyan

if (Test-Path $BuildDir) {
    try { Remove-Item -Recurse -Force $BuildDir -ErrorAction Stop }
    catch { Write-Host "  跳过: 部分文件被占用，继续使用现有 build 目录" -ForegroundColor Yellow }
}
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

& $CmakeExe -S $ProjectDir -B $BuildDir `
    -G "MinGW Makefiles" `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH="$QtDir" `
    -DCMAKE_MAKE_PROGRAM="$MakeExe"

if ($LASTEXITCODE -ne 0) { Write-Host "CMake 配置失败!" -ForegroundColor Red; exit 1 }

Write-Host "=== 2. 编译 Release ===" -ForegroundColor Cyan

& $CmakeExe --build $BuildDir --config Release
if ($LASTEXITCODE -ne 0) { Write-Host "编译失败!" -ForegroundColor Red; exit 1 }

Write-Host "=== 3. 打包 ===" -ForegroundColor Cyan

# 先关闭正在运行的 TASoftware，避免 DLL 被占用
$running = Get-Process -Name "TASoftware" -ErrorAction SilentlyContinue
if ($running) {
    Write-Host "  检测到 TASoftware 正在运行，正在关闭..." -ForegroundColor Yellow
    $running | Stop-Process -Force
    Start-Sleep -Milliseconds 500
}

if (Test-Path $PackageDir) {
    try { Remove-Item -Recurse -Force $PackageDir -ErrorAction Stop }
    catch { Write-Host "  跳过: 部分文件被占用，继续覆盖已有文件" -ForegroundColor Yellow }
}
New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null

Copy-Item (Join-Path $BuildDir "TASoftware.exe") $PackageDir -Force

& $WinDeploy (Join-Path $PackageDir "TASoftware.exe") `
    --no-translations --no-system-d3d-compiler

# 补上 MinGW 运行时 DLL
xcopy "$MingwBin\libgcc_s_seh-1.dll" "$PackageDir\" /Y /Q
xcopy "$MingwBin\libstdc++-6.dll"     "$PackageDir\" /Y /Q
xcopy "$MingwBin\libwinpthread-1.dll"  "$PackageDir\" /Y /Q

# 内嵌常用工具
$tools = @("Converter3.0.7.exe", "rtkplot.exe")
foreach ($tool in $tools) {
    $src = Join-Path $ProjectDir $tool
    if (Test-Path $src) {
        Copy-Item $src $PackageDir -Force
        Write-Host "  已复制: $tool" -ForegroundColor Gray
    } else {
        Write-Host "  跳过: $tool (未找到)" -ForegroundColor Yellow
    }
}

xcopy (Join-Path $ProjectDir "config.json") "$PackageDir\" /Y /Q
xcopy (Join-Path $ProjectDir "at_commands.json") "$PackageDir\" /Y /Q

# 复制 Python 解析脚本（凯芯数据解析依赖）
$scriptsSrc = Join-Path $ProjectDir "scripts"
$scriptsDst = Join-Path $PackageDir "scripts"
if (Test-Path $scriptsSrc) {
    New-Item -ItemType Directory -Force -Path $scriptsDst | Out-Null
    Copy-Item (Join-Path $scriptsSrc "*") $scriptsDst -Recurse -Force
    Write-Host "  已复制: scripts/" -ForegroundColor Gray
}

Write-Host ""
Write-Host "=== 打包完成 ===" -ForegroundColor Green
Write-Host "输出目录: $PackageDir"
Write-Host "运行: $PackageDir\TASoftware.exe"
