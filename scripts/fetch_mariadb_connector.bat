@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Determine repository root (resolve ".." relative to this script)
for %%I in ("%~dp0..") do set "REPO_ROOT=%%~fI"
set "THIRD_PARTY=%REPO_ROOT%\third_party"
set "DOWNLOADS=%THIRD_PARTY%\_downloads"
set "WIN_TARGET=%THIRD_PARTY%\mariadb-connector-c-3.3.8-win32"
set "LINUX_TARGET=%THIRD_PARTY%\mariadb-connector-c-3.3.8-linux"
set "WIN_MSI=%DOWNLOADS%\mariadb-connector-c-3.3.8-win32.msi"
set "LINUX_TARBALL=%DOWNLOADS%\mariadb-connector-c-3.3.8-ubuntu-jammy-amd64.tar.gz"
set "WIN_URL=https://downloads.mariadb.com/Connectors/c/connector-c-3.3.8/mariadb-connector-c-3.3.8-win32.msi"
set "LINUX_URL=https://downloads.mariadb.com/Connectors/c/connector-c-3.3.8/mariadb-connector-c-3.3.8-ubuntu-jammy-amd64.tar.gz"

echo Preparing third_party directory under %REPO_ROOT%
if not exist "%THIRD_PARTY%" (
    mkdir "%THIRD_PARTY%" || goto :fail
)
if not exist "%DOWNLOADS%" (
    mkdir "%DOWNLOADS%" || goto :fail
)

call :install_win
if errorlevel 1 goto :fail

call :install_linux
if errorlevel 1 goto :fail

echo.
echo MariaDB Connector/C packages installed under %THIRD_PARTY%.
exit /b 0

:install_win
if exist "%WIN_TARGET%\include\mysql.h" (
    echo [win32] MariaDB Connector/C already present, skipping download.
    goto :eof
)

echo [win32] Downloading %WIN_URL%
powershell -NoProfile -ExecutionPolicy Bypass -Command "$outFile = $env:WIN_MSI; if ([string]::IsNullOrWhiteSpace($outFile)) { throw 'WIN_MSI environment variable is empty.' } if (Test-Path -LiteralPath $outFile) { return }; $ProgressPreference='SilentlyContinue'; Invoke-WebRequest -Uri $env:WIN_URL -OutFile $outFile -UseBasicParsing"
if errorlevel 1 (
    echo [win32] Failed to download package.
    exit /b 1
)

set "WIN_TEMP=%DOWNLOADS%\win32"
if exist "%WIN_TEMP%" rd /s /q "%WIN_TEMP%"
mkdir "%WIN_TEMP%" || exit /b 1

echo [win32] Extracting MSI payload...
msiexec /a "%WIN_MSI%" TARGETDIR="%WIN_TEMP%" /qn
if errorlevel 1 (
    echo [win32] msiexec returned an error. Ensure Windows Installer is available and try again.
    exit /b 1
)

set "WIN_PAYLOAD="
for /f "usebackq delims=" %%I in (`powershell -NoProfile -Command "Get-ChildItem -Directory -Recurse -LiteralPath '%WIN_TEMP%' | Where-Object { Test-Path ([IO.Path]::Combine($_.FullName, 'include', 'mysql.h')) -and (Test-Path ([IO.Path]::Combine($_.FullName, 'lib', 'libmariadb.lib')) -or Test-Path ([IO.Path]::Combine($_.FullName, 'lib', 'mariadb', 'libmariadb.lib'))) } | Select-Object -First 1 -ExpandProperty FullName"`) do set "WIN_PAYLOAD=%%I"
if not defined WIN_PAYLOAD (
    echo [win32] Unable to locate extracted connector files.
    exit /b 1
)

if exist "%WIN_TARGET%" rd /s /q "%WIN_TARGET%"
mkdir "%WIN_TARGET%" || exit /b 1

echo [win32] Copying files into %WIN_TARGET%
robocopy "%WIN_PAYLOAD%" "%WIN_TARGET%" /E /NFL /NDL /NJH /NJS /NC /NS >nul
set "ROBOCODE=%ERRORLEVEL%"
if %ROBOCODE% GEQ 8 (
    echo [win32] robocopy failed with code %ROBOCODE%.
    exit /b 1
)

echo [win32] Connector installed successfully.
exit /b 0

:install_linux
if exist "%LINUX_TARGET%\include\mysql.h" (
    echo [linux] MariaDB Connector/C already present, skipping download.
    goto :eof
)

echo [linux] Downloading %LINUX_URL%
powershell -NoProfile -ExecutionPolicy Bypass -Command "$outFile = $env:LINUX_TARBALL; if ([string]::IsNullOrWhiteSpace($outFile)) { throw 'LINUX_TARBALL environment variable is empty.' } if (Test-Path -LiteralPath $outFile) { return }; $ProgressPreference='SilentlyContinue'; Invoke-WebRequest -Uri $env:LINUX_URL -OutFile $outFile -UseBasicParsing"
if errorlevel 1 (
    echo [linux] Failed to download package.
    exit /b 1
)

set "LINUX_TEMP=%DOWNLOADS%\linux"
if exist "%LINUX_TEMP%" rd /s /q "%LINUX_TEMP%"
mkdir "%LINUX_TEMP%" || exit /b 1

echo [linux] Extracting tarball...
powershell -NoProfile -ExecutionPolicy Bypass -Command "param([string]$Archive,[string]$Destination) if (-not (Get-Command tar -ErrorAction SilentlyContinue)) { Write-Error 'tar.exe was not found in PATH'; exit 1 } tar -xf $Archive -C $Destination" "%LINUX_TARBALL%" "%LINUX_TEMP%"
if errorlevel 1 (
    echo [linux] Failed to extract tarball. Install a tar utility (Windows 10+ ships one by default) and retry.
    exit /b 1
)

set "LINUX_PAYLOAD="
for /f "usebackq delims=" %%I in (`powershell -NoProfile -Command "Get-ChildItem -Directory -Recurse -LiteralPath '%LINUX_TEMP%' | Where-Object { Test-Path ([IO.Path]::Combine($_.FullName, 'include', 'mariadb', 'mysql.h')) -or Test-Path ([IO.Path]::Combine($_.FullName, 'include', 'mysql', 'mysql.h')) -or Test-Path ([IO.Path]::Combine($_.FullName, 'include', 'mysql.h')) } | Select-Object -First 1 -ExpandProperty FullName"`) do set "LINUX_PAYLOAD=%%I"
if not defined LINUX_PAYLOAD (
    echo [linux] Unable to locate extracted connector files.
    exit /b 1
)

if exist "%LINUX_TARGET%" rd /s /q "%LINUX_TARGET%"
mkdir "%LINUX_TARGET%" || exit /b 1

echo [linux] Copying files into %LINUX_TARGET%
robocopy "%LINUX_PAYLOAD%" "%LINUX_TARGET%" /E /NFL /NDL /NJH /NJS /NC /NS >nul
set "ROBOCODE=%ERRORLEVEL%"
if %ROBOCODE% GEQ 8 (
    echo [linux] robocopy failed with code %ROBOCODE%.
    exit /b 1
)

echo [linux] Connector installed successfully.
exit /b 0

:fail
echo.
echo Dependency download failed. See messages above for details.
exit /b 1
