@echo off
echo === minipro-t76 WinUSB Driver Uninstaller ===
echo.

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Must run as Administrator!
    pause
    exit /b 1
)

echo Removing minipro-t76 WinUSB driver...
pnputil /delete-driver "%~dp0T76_WinUSB.inf" /uninstall /force

echo.
echo Driver removed. Unplug and re-plug the T76.
echo Windows will revert to the default driver (or no driver).
echo.
pause
