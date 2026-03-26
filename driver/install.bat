@echo off
echo === minipro-t76 WinUSB Driver Installer ===
echo.
echo This installs a clean WinUSB driver for the XGecu T76 Programmer.
echo Uses ONLY Microsoft's built-in WinUSB.sys - no custom binaries.
echo.
echo Right-click this file and "Run as Administrator" if not already.
echo.

:: Check admin
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Must run as Administrator!
    echo Right-click this file and select "Run as administrator"
    pause
    exit /b 1
)

:: Install the driver using pnputil
echo Installing driver...
pnputil /add-driver "%~dp0T76_WinUSB.inf" /install

if %errorlevel% equ 0 (
    echo.
    echo SUCCESS! Driver installed.
    echo Unplug and re-plug the T76 programmer.
) else (
    echo.
    echo Driver install via pnputil failed. Trying devcon...
    echo If this also fails, use Device Manager manually:
    echo   1. Open Device Manager
    echo   2. Find the T76 (may be under "Other Devices")
    echo   3. Right-click ^> Update Driver
    echo   4. Browse my computer ^> Select this folder
)

echo.
pause
