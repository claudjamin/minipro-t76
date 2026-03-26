minipro-t76 WinUSB Driver for XGecu T76
========================================

This is a clean, minimal WinUSB driver for the XGecu T76 programmer.

WHAT THIS DRIVER IS:
- A standard Microsoft WinUSB.sys driver configuration
- Only an .inf file that tells Windows to use its built-in WinUSB.sys
- Matches the T76's USB VID:A466 PID:1A86
- Uses the same device interface GUID as the stock XGecu driver
- Compatible with libusb, WinUSB API, and usbipd (WSL)

WHAT THIS DRIVER IS NOT:
- No custom kernel driver (.sys file)
- No Chinese software bundled
- No network access
- No telemetry or phone-home
- No auto-updater

INSTALL:
  Option 1: Run install.bat as Administrator
  Option 2: Device Manager > Update Driver > Browse > Select this folder
  Option 3: pnputil /add-driver T76_WinUSB.inf /install

UNINSTALL:
  Run uninstall.bat as Administrator

WHY IS THIS NEEDED:
  The T76 is USB 3.0 SuperSpeed. Without the WinUSB driver installed,
  the USB communication doesn't work properly through usbipd/WSL or
  some VM passthrough configurations. The WinUSB driver ensures proper
  USB 3.0 endpoint initialization.

UNSIGNED DRIVER NOTE:
  This driver is unsigned. Windows may require you to:
  1. Disable driver signature enforcement (boot menu)
  2. Or enable test signing: bcdedit /set testsigning on
  3. Or use the stock XGecu driver installer instead

  The stock XGecu driver (UsbDriverInstall.exe) installs a signed
  version of this same WinUSB configuration. The only difference is
  the .cat signature files and the class GUID.
