# minipro-t76

Open source Linux CLI programmer for the **XGecu T76** universal programmer.

Drop-in replacement for the Windows-only Xgpro software -- no phone-home, no telemetry, no beaconing to `xgecu.com`. Just direct USB communication via libusb.

## Features

- **Read/Write/Erase/Verify** chips via command line
- **23,800+ chip database** extracted from Xgpro V13.17
- **Adapter setup images** -- shows which adapter to use and chip placement (`-a` flag)
- **Intel HEX, SREC, and binary** file format support
- **SPI flash autodetect**
- **No network access** -- zero connections to the internet, ever

## Build

```bash
# Install dependencies
sudo apt install build-essential libusb-1.0-0-dev

# Clone and build
git clone https://github.com/claudjamin/minipro-t76.git
cd minipro-t76
make

# (Optional) Install system-wide
sudo make install    # binary to /usr/local/bin
sudo make udev       # udev rule for non-root USB access
```

## Hardware Setup

The T76 connects via USB. On Linux:

```bash
# Verify the programmer is detected
lsusb | grep a466
# Expected: Bus XXX Device XXX: ID a466:1a86

# If running in a VM (VMware/VirtualBox):
# 1. Connect the T76 to the host via USB
# 2. In VM settings, pass through the USB device (VID:A466 PID:1A86)
# 3. Verify with lsusb inside the VM
```

If you get permission errors without sudo, install the udev rule:
```bash
sudo make udev
# Then unplug and replug the programmer
```

---

## Detailed Usage Guide

### Quick Reference

| Action | Command |
|--------|---------|
| Show programmer info | `sudo ./minipro-t76 -i` |
| List all chips | `./minipro-t76 -l` |
| Search for a chip | `./minipro-t76 -l W25Q*` |
| Show adapter setup | `./minipro-t76 -p "W25Q32 @SOIC8" -a` |
| Read chip ID | `sudo ./minipro-t76 -p "W25Q32 @SOIC8" -d` |
| Read chip to file | `sudo ./minipro-t76 -p "W25Q32 @SOIC8" -r dump.bin` |
| Write file to chip | `sudo ./minipro-t76 -p "W25Q32 @SOIC8" -w firmware.bin` |
| Erase chip | `sudo ./minipro-t76 -p "W25Q32 @SOIC8" -e` |
| Erase + Write | `sudo ./minipro-t76 -p "W25Q32 @SOIC8" -e -w firmware.bin` |
| Verify chip vs file | `sudo ./minipro-t76 -p "W25Q32 @SOIC8" -m firmware.bin` |

### Step-by-Step: Dumping Firmware from a SPI Flash Chip

This is the most common use case -- reading the contents of a flash chip for backup, analysis, or modification.

**Step 1: Identify the chip**

Look at the markings on the chip. Common SPI flash chips:
- `W25Q32` / `W25Q64` / `W25Q128` (Winbond)
- `MX25L6405` / `MX25L12835` (Macronix)
- `GD25Q64` / `GD25Q128` (GigaDevice)
- `EN25QH64` (EON)

Note the package type (usually SOIC8 for 8-pin surface mount).

**Step 2: Find the chip in the database**

```bash
# Search by partial name
./minipro-t76 -l W25Q32

# Use wildcards
./minipro-t76 -l "W25Q*"

# Search all Winbond chips
./minipro-t76 -l "W25*"

# Search by package type
./minipro-t76 -l "@SOIC8"
```

Pick the exact match including the package suffix (e.g., `W25Q32 @SOIC8`).

**Step 3: Check the adapter setup**

```bash
./minipro-t76 -p "W25Q32 @SOIC8" -a
```

This shows an image of how to connect the chip to the programmer. For SOIC8 SPI flash, you either:
- Place the chip in the ZIF socket using an adapter board
- Use an SOIC8 test clip to read the chip in-circuit (ISP)

**Step 4: Verify the programmer sees the chip**

```bash
sudo ./minipro-t76 -p "W25Q32 @SOIC8" -d
```

This reads the JEDEC chip ID. You should see something like:
```
Chip ID: 0xEF4016
  Manufacturer: 0xEF    (Winbond)
  Device: 0x4016        (W25Q32)
```

If the ID is `0x000000` or `0xFFFFFF`, the chip is not connected properly.

**Step 5: Read/dump the chip**

```bash
# Read to binary file (most common)
sudo ./minipro-t76 -p "W25Q32 @SOIC8" -r firmware_dump.bin

# Read to Intel HEX format
sudo ./minipro-t76 -p "W25Q32 @SOIC8" -r firmware_dump.hex -f ihex

# Read to Motorola S-Record format
sudo ./minipro-t76 -p "W25Q32 @SOIC8" -r firmware_dump.srec -f srec
```

**Step 6: Verify the dump is good**

Always read twice and compare to make sure the dump is consistent:

```bash
sudo ./minipro-t76 -p "W25Q32 @SOIC8" -r dump1.bin
sudo ./minipro-t76 -p "W25Q32 @SOIC8" -r dump2.bin
md5sum dump1.bin dump2.bin
# Both hashes should match
```

Or use the built-in verify:
```bash
sudo ./minipro-t76 -p "W25Q32 @SOIC8" -m dump1.bin
```

### Step-by-Step: Writing Firmware to a Chip

**Step 1: Erase the chip first**

Most flash chips must be erased before writing. Erasing sets all bytes to `0xFF`.

```bash
sudo ./minipro-t76 -p "W25Q32 @SOIC8" -e
```

**Step 2: Write the firmware**

```bash
# Write a binary file
sudo ./minipro-t76 -p "W25Q32 @SOIC8" -w firmware.bin

# Write an Intel HEX file (auto-detected)
sudo ./minipro-t76 -p "W25Q32 @SOIC8" -w firmware.hex

# Erase + write in one command (recommended)
sudo ./minipro-t76 -p "W25Q32 @SOIC8" -e -w firmware.bin
```

The tool automatically verifies after writing. If verification fails, the chip contents don't match what was written -- check your connections.

**Step 3: Protect the chip (optional)**

Some chips support write protection to prevent accidental modification:

```bash
sudo ./minipro-t76 -p "W25Q32 @SOIC8" -P
```

### Step-by-Step: Working with TSOP48 Parallel Flash

Parallel flash chips (NOR flash in TSOP48 packages) are found on routers, BIOS chips, and embedded devices.

```bash
# Find your chip
./minipro-t76 -l "MX29*"
./minipro-t76 -l "AM29*"
./minipro-t76 -l "SST39*"

# Check adapter placement (you need the TSOP48 adapter board)
./minipro-t76 -p "MX29LV640EB @TSOP48" -a

# Read
sudo ./minipro-t76 -p "MX29LV640EB @TSOP48" -r nor_dump.bin

# Erase + Write
sudo ./minipro-t76 -p "MX29LV640EB @TSOP48" -e -w modified_firmware.bin
```

### Step-by-Step: Working with EEPROMs

EEPROMs (24Cxx, 93Cxx series) are small serial memory chips used for configuration storage.

```bash
# Find the chip
./minipro-t76 -l "24C*"
./minipro-t76 -l "93C*"

# Read a 24C256 EEPROM
sudo ./minipro-t76 -p "AT24C256 @DIP8" -r eeprom_data.bin

# Write new data
sudo ./minipro-t76 -p "AT24C256 @DIP8" -w eeprom_data.bin

# EEPROMs usually don't need erasing before writing
```

### Step-by-Step: Working with DIP Package Chips

DIP (Dual Inline Package) chips plug directly into the ZIF socket -- no adapter needed.

```bash
# Check placement
./minipro-t76 -p "AT28C256 @DIP28" -a

# The image shows: place pin 1 at the top of the ZIF socket
# Pin 1 is marked with a dot or notch on the chip

# Read
sudo ./minipro-t76 -p "AT28C256 @DIP28" -r contents.bin

# Write
sudo ./minipro-t76 -p "AT28C256 @DIP28" -e -w new_data.bin
```

### Common Troubleshooting

**"XGecu T76 not found"**
- Is the programmer plugged in? Check `lsusb | grep a466`
- If in a VM, did you pass the USB device through?
- Try a different USB cable or port
- On WSL2: you need `usbipd` to forward USB devices

**Chip ID reads as 0x000000 or 0xFFFFFF**
- Chip not seated properly in the socket/adapter
- Wrong chip selected (try autodetect with `-d`)
- Chip is dead or write-protected
- Check the adapter setup image (`-a`)

**Verification fails after writing**
- Bad contact -- reseat the chip
- Chip needs to be erased first (`-e`)
- Write protection is enabled -- use `-u` to unprotect first:
  ```bash
  sudo ./minipro-t76 -p "W25Q32 @SOIC8" -u -e -w firmware.bin
  ```
- Some chips have limited write cycles (especially older EEPROMs)

**"Error: chip not found in database"**
- Check exact spelling: `./minipro-t76 -l "YOUR_CHIP*"`
- Include the package suffix: `-p "W25Q32 @SOIC8"` not just `-p W25Q32`
  (without package suffix, it picks the first match)
- Use a custom database file: `-D /path/to/chipdb.txt`

### All Command-Line Options

```
Usage: minipro-t76 [options]

Device operations:
  -p <chip>        Select chip by name (use quotes if name has spaces)
  -r <file>        Read chip contents to file
  -w <file>        Write file contents to chip
  -e               Erase chip
  -m <file>        Verify chip contents against file
  -d               Detect/read chip JEDEC ID
  -u               Unprotect chip before operation
  -P               Protect chip after operation

Adapter/Setup:
  -a               Show adapter setup image for the selected chip
  -I <dir>         Use alternate image directory

File format:
  -f <format>      Force file format: bin, ihex, srec (default: auto-detect)

Database:
  -l [filter]      List chips (supports glob patterns like W25Q* or substring search)
  -D <path>        Use alternate chip database file

Programmer:
  -i               Show programmer hardware/firmware info
  -v               Verbose output
  -h, --help       Show help
  -V, --version    Show version
```

### File Formats

| Format | Extension | Description |
|--------|-----------|-------------|
| Binary | `.bin` | Raw binary, byte-for-byte copy of chip contents (default) |
| Intel HEX | `.hex`, `.ihex` | ASCII hex with addresses, used by many MCU tools |
| SREC | `.srec`, `.s19`, `.mot` | Motorola S-Record, common in embedded/automotive |

The format is auto-detected on read based on file content. For write output, use `-f` to specify:
```bash
sudo ./minipro-t76 -p W25Q32 -r dump.hex -f ihex
```

---

## Protocol

Based on the verified USB protocol from the [minipro](https://gitlab.com/DavidGriffith/minipro) open-source project.

| Endpoint | Direction | Purpose |
|----------|-----------|---------|
| 0x01 | OUT | Command messages |
| 0x81 | IN | Command responses |
| 0x05 | OUT | Bulk data write (T76-specific) |
| 0x82 | IN | Bulk data read (T76-specific) |

## Hardware

- **MCU:** WCH CH569 (RISC-V, USB 3.0 SuperSpeed)
- **FPGA:** Anlogic EG4X20BG256
- **USB:** VID `0xA466`, PID `0x1A86`

## Credits

- Protocol: [minipro project](https://gitlab.com/DavidGriffith/minipro) by David Griffith and radiomanV
- T76 hardware docs: [radiomanV/Xgecu_T76](https://github.com/radiomanV/Xgecu_T76)
- Chip database: Extracted from XGecu InfoICT76.dll

## License

GPL-3.0 (same as minipro)
