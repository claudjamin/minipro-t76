# minipro-t76 - Build & Install

## Dependencies

```bash
# Debian/Ubuntu/Kali
sudo apt install build-essential libusb-1.0-0-dev

# Fedora
sudo dnf install gcc make libusb1-devel

# Arch
sudo pacman -S libusb
```

## Build

```bash
make
```

## Install (optional)

```bash
sudo make install       # Installs binary to /usr/local/bin
sudo make udev          # Installs udev rule for non-root access
```

## Extract chip database (if chipdb.txt is missing)

```bash
make tools
./tools/extract_chipdb /path/to/InfoICT76.dll chipdb.txt
```

## Quick test

```bash
# Show programmer info
./minipro-t76 -i

# List supported chips
./minipro-t76 -l W25Q*

# USB debug/sniff mode (for protocol development)
./minipro-t76 --sniff
```

## Usage

```bash
# Read a chip
./minipro-t76 -p "W25Q32 @SOIC8" -r dump.bin

# Write a chip (auto-verifies after write)
./minipro-t76 -p W25Q32 -w firmware.bin

# Erase then write
./minipro-t76 -p W25Q32 -e -w firmware.bin

# Blank check
./minipro-t76 -p W25Q32 -b

# Detect chip ID
./minipro-t76 -p W25Q32 -d
```

## Protocol Development

The `--sniff` mode sends probe commands and dumps raw USB traffic.
This is essential for refining the command byte values (see `include/t76.h`).

To capture USB traffic from the Windows Xgpro software:
1. Use Wireshark with USBPcap on Windows
2. Or use `usbmon` on Linux with the device passed through to a VM
3. Compare captured traffic against the command definitions in `t76.h`

## No Phone-Home

Unlike the stock Xgpro software, this tool:
- Makes ZERO network connections
- Has no telemetry or update checks
- Does not beacon to xgecu.com or anywhere else
- Communicates only with the USB device
