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

# Qt6 cmake_autogen opens AutogenInfo.json with a narrow (ANSI) path, so a
# build dir containing non-ASCII chars fails with "info file ... is not readable".
# When the project path is non-ASCII, keep sources where they are but build
# into an ASCII-only directory.
if ($ProjectDir -match '[^\x00-\x7F]') {
    $asciiBase = Join-Path $env:LOCALAPPDATA "TASoftwareBuild"
    if ($asciiBase -match '[^\x00-\x7F]') { $asciiBase = "C:\TASoftwareBuild" }
    $BuildDir = Join-Path $asciiBase "release"
    Write-Host "  Non-ASCII project path, build dir -> $BuildDir" -ForegroundColor Yellow
}

$QtDir     = "B:/qt/6.8.3/mingw_64"
$CmakeExe  = "B:/qt/Tools/CMake_64/bin/cmake.exe"
$MakeExe   = "B:/qt/Tools/mingw1310_64/bin/mingw32-make.exe"
$MingwBin  = "B:/qt/Tools/mingw1310_64/bin"
$WinDeploy = "B:/qt/6.8.3/mingw_64/bin/windeployqt.exe"

$env:PATH = "B:/qt/Tools/CMake_64/bin;B:/qt/Tools/mingw1310_64/bin;B:/qt/6.8.3/mingw_64/bin;$env:PATH"

$Jobs = [Math]::Max(4, ($env:NUMBER_OF_PROCESSORS - 1))
Write-Host "  Parallel: -j $Jobs  (cores: $env:NUMBER_OF_PROCESSORS)" -ForegroundColor Gray

# After the project folder was moved, an old CMakeCache.txt still records the
# previous source dir, which makes make fail in cmake_check_build_system.
# So do not rely on the Makefile alone: also verify CMAKE_HOME_DIRECTORY.
$cacheFile = Join-Path $BuildDir "CMakeCache.txt"
$staleCache = $false
if (Test-Path $cacheFile) {
    $cachedHome = Select-String -Path $cacheFile -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$' |
                  ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() } |
                  Select-Object -First 1
    if ($cachedHome) {
        $normOld = ($cachedHome -replace '\\', '/').TrimEnd('/').ToLower()
        $normNew = ($ProjectDir -replace '\\', '/').TrimEnd('/').ToLower()
        if ($normOld -ne $normNew) {
            $staleCache = $true
            Write-Host "  Stale cache: $cachedHome" -ForegroundColor Yellow
        }
    }
}

$NeedConfigure = $Clean.IsPresent -or $staleCache -or -not (Test-Path (Join-Path $BuildDir "Makefile"))

if ($NeedConfigure) {
    Write-Host "=== 1. CMake configure ===" -ForegroundColor Cyan
    if ($Clean.IsPresent -and (Test-Path $BuildDir)) {
        try { Remove-Item -Recurse -Force $BuildDir -ErrorAction Stop }
        catch { Write-Host "  Skip: some files locked" -ForegroundColor Yellow }
    }
    # Source dir changed: drop the stale cache first, otherwise cmake refuses
    # to configure into a build dir created for another source tree.
    if ($staleCache -and (Test-Path $cacheFile)) {
        try {
            Remove-Item -Force $cacheFile -ErrorAction Stop
            Remove-Item -Recurse -Force (Join-Path $BuildDir "CMakeFiles") -ErrorAction SilentlyContinue
            Write-Host "  Removed stale CMakeCache.txt" -ForegroundColor Gray
        } catch { Write-Host "  Skip: cache locked, delete $cacheFile manually" -ForegroundColor Yellow }
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
    try {
        $running | Stop-Process -Force -ErrorAction Stop
    } catch {
        # Access denied means Witt was started elevated while this shell is not.
        # taskkill needs the same privileges, so relaunch it through UAC - but a
        # process whose image path is unreadable is already stuck/zombie and no
        # amount of privilege will help, so skip the pointless UAC prompt.
        $elevatable = @($running | Where-Object { try { $_.Path } catch { $null } })
        if ($elevatable) {
            Write-Host "  Access denied, retrying with administrator privileges..." -ForegroundColor Yellow
            foreach ($p in $elevatable) {
                try {
                    Start-Process -FilePath "$env:SystemRoot\System32\taskkill.exe" `
                        -ArgumentList "/F", "/PID", $p.Id -Verb RunAs -Wait -ErrorAction Stop
                } catch {
                    Write-Host "  Elevated kill failed or was cancelled." -ForegroundColor Yellow
                }
            }
        } else {
            Write-Host "  Process seems stuck (image path unreadable), skipping elevated kill." -ForegroundColor Yellow
        }
    }
    Start-Sleep -Milliseconds 500
    $stillRunning = Get-Process -Name "Witt" -ErrorAction SilentlyContinue
    if ($stillRunning) {
        $ids = ($stillRunning | ForEach-Object { $_.Id }) -join ', '
        # A process that survives Stop-Process and elevated taskkill is stuck in
        # kernel mode (hung driver / mid-termination). It no longer holds the
        # package files, so keep packaging and let the copy below report locks.
        Write-Host "  Witt (PID $ids) survived the kill - stuck process." -ForegroundColor Yellow
        Write-Host "  Continuing packaging; reboot to clear it if the copy fails." -ForegroundColor Yellow
    }
}

if (Test-Path $PackageDir) {
    try { Remove-Item -Recurse -Force $PackageDir -ErrorAction Stop }
    catch { Write-Host "  Skip: some files locked" -ForegroundColor Yellow }
}
New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null

try {
    Copy-Item (Join-Path $BuildDir "Witt.exe") $PackageDir -Force -ErrorAction Stop
} catch {
    Write-Host "  Copy failed: Witt.exe is locked. Close Witt (or reboot) and rerun." -ForegroundColor Red
    exit 1
}

& $WinDeploy (Join-Path $PackageDir "Witt.exe") `
    --no-translations --no-system-d3d-compiler

# xcopy chokes on the forward slashes in $MingwBin (taken as switches), so copy
# these with Copy-Item instead.
foreach ($dll in "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll") {
    Copy-Item (Join-Path $MingwBin $dll) $PackageDir -Force
}

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
