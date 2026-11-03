# Witt build script
# Usage:
#   .\package.ps1           # incremental build (default)
#   .\package.ps1 -Clean    # clean rebuild

param(
    [switch]$Clean
)

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

$Jobs = [Math]::Max(4, ($env:NUMBER_OF_PROCESSORS - 1))
Write-Host "  Parallel: -j $Jobs  (cores: $env:NUMBER_OF_PROCESSORS)" -ForegroundColor Gray

$NeedConfigure = $Clean.IsPresent -or -not (Test-Path (Join-Path $BuildDir "Makefile"))

if ($NeedConfigure) {
    Write-Host "=== 1. CMake configure ===" -ForegroundColor Cyan
    if ($Clean.IsPresent -and (Test-Path $BuildDir)) {
        try { Remove-Item -Recurse -Force $BuildDir -ErrorAction Stop }
        catch { Write-Host "  Skip: some files locked" -ForegroundColor Yellow }
    }
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

    & $CmakeExe -S $ProjectDir -B $BuildDir `
        -G "MinGW Makefiles" `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_PREFIX_PATH="$QtDir" `
        -DCMAKE_MAKE_PROGRAM="$MakeExe"

    if ($LASTEXITCODE -ne 0) { Write-Host "CMake configure failed!" -ForegroundColor Red; exit 1 }
} else {
    Write-Host "=== 1. CMake configure (skip, using existing Makefile) ===" -ForegroundColor Gray
}

Write-Host "=== 2. Build Release (-j $Jobs) ===" -ForegroundColor Cyan

$buildTimer = Measure-Command {
    & $CmakeExe --build $BuildDir --config Release -- -j $Jobs
}
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed!" -ForegroundColor Red; exit 1 }
Write-Host "  Build time: $([math]::Round($buildTimer.TotalSeconds, 1))s" -ForegroundColor Gray

Write-Host "=== 3. Package ===" -ForegroundColor Cyan

$running = Get-Process -Name "Witt" -ErrorAction SilentlyContinue
if ($running) {
    Write-Host "  Witt running, stopping..." -ForegroundColor Yellow
    $running | Stop-Process -Force
    Start-Sleep -Milliseconds 500
}

if (Test-Path $PackageDir) {
    try { Remove-Item -Recurse -Force $PackageDir -ErrorAction Stop }
    catch { Write-Host "  Skip: some files locked" -ForegroundColor Yellow }
}
New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null

Copy-Item (Join-Path $BuildDir "Witt.exe") $PackageDir -Force

& $WinDeploy (Join-Path $PackageDir "Witt.exe") `
    --no-translations --no-system-d3d-compiler

xcopy "$MingwBin\libgcc_s_seh-1.dll" "$PackageDir\" /Y /Q
xcopy "$MingwBin\libstdc++-6.dll"     "$PackageDir\" /Y /Q
xcopy "$MingwBin\libwinpthread-1.dll"  "$PackageDir\" /Y /Q

$pcanDll = Join-Path $env:SystemRoot "System32\PCANBasic.dll"
if (Test-Path $pcanDll) {
    Copy-Item $pcanDll $PackageDir -Force
    Write-Host "  Copied: PCANBasic.dll" -ForegroundColor Gray
} else {
    Write-Host "  Skip: PCANBasic.dll (not found)" -ForegroundColor Yellow
}

$tools = @("Converter3.0.7.exe", "rtkplot.exe")
foreach ($tool in $tools) {
    $src = Join-Path $ProjectDir $tool
    if (Test-Path $src) {
        Copy-Item $src $PackageDir -Force
        Write-Host "  Copied: $tool" -ForegroundColor Gray
    }
}

xcopy (Join-Path $ProjectDir "config.json") "$PackageDir\" /Y /Q
xcopy (Join-Path $ProjectDir "at_commands.json") "$PackageDir\" /Y /Q
xcopy (Join-Path $ProjectDir "sim_protocols.json") "$PackageDir\" /Y /Q

$scriptsSrc = Join-Path $ProjectDir "scripts"
$scriptsDst = Join-Path $PackageDir "scripts"
if (Test-Path $scriptsSrc) {
    New-Item -ItemType Directory -Force -Path $scriptsDst | Out-Null
    Copy-Item (Join-Path $scriptsSrc "*") $scriptsDst -Recurse -Force
    Write-Host "  Copied: scripts/" -ForegroundColor Gray
}

Write-Host ""
Write-Host "=== Done ===" -ForegroundColor Green
Write-Host "Output: $PackageDir"
Write-Host "Run: $PackageDir\Witt.exe"
