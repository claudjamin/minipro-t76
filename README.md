# minipro-t76

Open source Linux CLI programmer for the **XGecu T76** universal programmer.

Drop-in replacement for the Windows-only Xgpro software -- no phone-home, no telemetry, no beaconing to `xgecu.com`. Just direct USB communication via libusb.

## Features

- **65,178 chip database** converted from minipro's `infoic.xml` with full parameters
- **NAND flash support** -- reverse-engineered streaming protocol, 264MB dump in 15 seconds
- **Read/Write/Erase/Verify** chips via command line
- **Adapter setup images** -- shows which adapter to use and chip placement (`-a` flag)
- **Chip connectivity test** (`-z`) -- verifies chip-to-socket contact before operations
- **Intel HEX, SREC, and binary** file format support
- **SPI flash autodetect**
- **Verbose/debug modes** (`-v` info, `-vv` full USB hex dumps for protocol analysis)
- **Windows native NAND reader** (`nand_final.exe`) for USB 3.0 SuperSpeed transfers
- **Clean WinUSB driver** included -- no Chinese software needed
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

## Extracting Required Files from Xgpro (Windows Installer)

minipro-t76 needs two sets of files from the official Windows Xgpro installer that are **not included in this repo** due to size and licensing:

1. **`algoT76/`** -- FPGA algorithm/bitstream files (362 files, ~66MB). **Required** -- without these, the programmer cannot talk to any chip.
2. **`img/`** -- Adapter setup images (138 files, ~7MB). Optional but helpful.

### Where to Get the Xgpro Installer

| Source | Link | Notes |
|--------|------|-------|
| **GitHub Mirror** (recommended) | https://github.com/Kreeblah/XGecu_Software/tree/master/Xgpro/13 | GitHub-hosted mirror, no China firewall issues |
| **Direct download (V13.17)** | https://github.com/Kreeblah/XGecu_Software/raw/refs/heads/master/Xgpro/13/xgpro_T76_V1317.rar | Latest T76 installer (~65MB) |
| **Direct download (V13.11)** | https://github.com/Kreeblah/XGecu_Software/raw/refs/heads/master/Xgpro/13/xgpro_T76_V1311.rar | Previous version |
| **XGecu Official** | http://www.xgecu.com/en/Download.html | May be slow/blocked (hosted in China) |
| **Minipro project** | https://gitlab.com/DavidGriffith/minipro | Open-source alternative -- includes its own chip database and algorithm files, no Xgpro needed |
| **Minipro releases** | https://661.org/files/minipro/ | Pre-built tarballs |
| **T76 hardware docs** | https://github.com/radiomanV/Xgecu_T76 | FPGA pin maps, schematics, bitstream tools |

**Quick download and extract:**
```bash
# Download from GitHub mirror
wget https://github.com/Kreeblah/XGecu_Software/raw/refs/heads/master/Xgpro/13/xgpro_T76_V1317.rar

# The RAR contains a self-extracting EXE which contains the actual files
# Extract twice: RAR -> EXE -> files
mkdir xgpro_tmp && unrar x xgpro_T76_V1317.rar xgpro_tmp/
cd xgpro_tmp && unrar x Xgpro_T76_V1317.exe && cd ..

# Copy algorithm files (REQUIRED) and images (optional) into your minipro-t76 dir
cp -r xgpro_tmp/algoT76 /path/to/minipro-t76/
cp -r xgpro_tmp/img /path/to/minipro-t76/

# Clean up
rm -rf xgpro_tmp xgpro_T76_V1317.rar
```

**Alternative: Use minipro directly.** The open-source [minipro](https://gitlab.com/DavidGriffith/minipro) project fully supports the T76 and bundles its own chip database (`infoic.xml`) and algorithm files. It doesn't need any files from Xgpro:
```bash
git clone https://gitlab.com/DavidGriffith/minipro.git
cd minipro
sudo apt install build-essential libusb-1.0-0-dev pkg-config bsdtar
make
sudo make install
sudo minipro -p "W25Q32JV @SOIC8" -r dump.bin
```

### Method 1: Extract on Linux (recommended)

```bash
# Install unrar if needed
sudo apt install unrar

# If you have the .rar file (e.g. xgpro_T76_V1317.rar):
# Step 1: Extract the RAR (contains a self-extracting EXE)
mkdir xgpro_extracted
unrar x xgpro_T76_V1317.rar xgpro_extracted/

# Step 2: Extract the EXE (it's also a RAR archive with the actual files)
cd xgpro_extracted
unrar x Xgpro_T76_V1317.exe
cd ..

# If you already have the .exe directly:
# mkdir xgpro_extracted && unrar x Xgpro_T76_V1317.exe xgpro_extracted/

# Copy the algorithm files (REQUIRED)
cp -r xgpro_extracted/algoT76 /path/to/minipro-t76/

# Copy the adapter images (optional)
cp -r xgpro_extracted/img /path/to/minipro-t76/

# Verify
ls minipro-t76/algoT76/ | wc -l    # should show 362
ls minipro-t76/img/ | wc -l        # should show 138
```

### Method 2: Extract on Windows, copy to Linux

If you already have Xgpro installed on Windows:

```
# The default install path on Windows is:
D:\Xgpro_T76\

# The files you need:
D:\Xgpro_T76\algoT76\      (all .alg files)
D:\Xgpro_T76\img\           (all .jpg files)
```

Copy them to your Linux machine:
```bash
# Via SCP from Windows (using Git Bash, PowerShell, or WSL):
scp -r "D:\Xgpro_T76\algoT76" user@linux-machine:/path/to/minipro-t76/
scp -r "D:\Xgpro_T76\img" user@linux-machine:/path/to/minipro-t76/

# Via USB drive:
# Just copy the algoT76/ and img/ folders to a USB stick,
# then copy them into the minipro-t76 directory on Linux

# Via shared folder (VMware):
cp -r /mnt/hgfs/shared/algoT76 /path/to/minipro-t76/
cp -r /mnt/hgfs/shared/img /path/to/minipro-t76/
```

### Method 3: From WSL (if Xgpro is installed on the same Windows machine)

```bash
# WSL can access Windows drives directly
cp -r /mnt/c/Users/YOUR_USER/path/to/Xgpro_T76/algoT76 /path/to/minipro-t76/
cp -r /mnt/c/Users/YOUR_USER/path/to/Xgpro_T76/img /path/to/minipro-t76/

# Or if you extracted the RAR in WSL:
cp -r /mnt/c/Users/YOUR_USER/claude/xgpro_extracted/app/algoT76 /path/to/minipro-t76/
cp -r /mnt/c/Users/YOUR_USER/claude/xgpro_extracted/app/img /path/to/minipro-t76/
```

### Method 4: Extract with 7-Zip on Windows

1. Right-click `Xgpro_T76_V1317.exe` → **7-Zip** → **Extract Here** (or **Open Archive**)
2. Inside you'll see the `algoT76/` and `img/` folders
3. Copy them to your Linux machine using any method above

### Verify Everything Works

```bash
cd minipro-t76

# Check algorithm files are present
ls algoT76/*.alg | head -5
# Should show: T7_28F32P78.alg, T7_AT45D31.alg, etc.

# Check images are present (optional)
ls img/*.jpg | head -5
# Should show: Adapter001.jpg, NoAdapter.jpg, etc.

# Test
sudo ./minipro-t76 -p "W25Q32 @SOIC8" -d
# Should now load an algorithm and attempt to read chip ID
```

## Hardware Setup

The T76 connects via USB. On Linux:

```bash
# Verify the programmer is detected
lsusb | grep a466
# Expected: Bus XXX Device XXX: ID a466:1a86

# If running in a VM (VMware/VirtualBox):
# 1. Connect the T76 to the host via USB
# 2. In VM menu: VM > Removable Devices > XGecu T76 > Connect
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
| Pin contact test | `sudo ./minipro-t76 -p "W25Q32 @SOIC8" -z` |
| Read chip ID | `sudo ./minipro-t76 -p "W25Q32 @SOIC8" -d` |
| Read chip to file | `sudo ./minipro-t76 -p "W25Q32 @SOIC8" -r dump.bin` |
| Write file to chip | `sudo ./minipro-t76 -p "W25Q32 @SOIC8" -w firmware.bin` |
| Erase chip | `sudo ./minipro-t76 -p "W25Q32 @SOIC8" -e` |
| Erase + Write | `sudo ./minipro-t76 -p "W25Q32 @SOIC8" -e -w firmware.bin` |
| Verify chip vs file | `sudo ./minipro-t76 -p "W25Q32 @SOIC8" -m firmware.bin` |
| Verbose output | Add `-v` to any command |
| Full USB hex dumps | Add `-vv` to any command |

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

### Step-by-Step: Reading NAND Flash

NAND flash chips (MT29F, K9F, etc.) use a different protocol than SPI/NOR flash. The tool handles this automatically when the chip type is NAND.

**Supported NAND chips:** Currently verified with MT29F2G08 (Micron 2Gbit, 256MB + spare). Other NAND chips in the database should work if they use the same page geometry.

**Step 1: Find your chip**

```bash
./minipro-t76 -l "MT29F*"
./minipro-t76 -l "K9F*"       # Samsung NAND
./minipro-t76 -l "TC58*"      # Kioxia/Toshiba NAND
```

**Step 2: Check adapter setup**

Most NAND chips use the TSOP48 adapter:
```bash
./minipro-t76 -p "MT29F2G08ABDWP@TSOP48" -a
```

**Step 3: Read the chip**

```bash
# Reads the full NAND including spare/OOB area
sudo ./minipro-t76 -p "MT29F2G08ABDWP@TSOP48" -r nand_dump.bin
```

The dump includes both data and spare (OOB) bytes for every page. For MT29F2G08:
- Page size: 2048 bytes data + 64 bytes spare = 2112 bytes/page
- 64 pages per erase block, 2048 erase blocks
- Total dump: 2048 x 64 x 2112 = 276,824,064 bytes (~264 MB)

**Step 4: Extract data-only content (optional)**

The dump contains interleaved data+spare. To extract just the data bytes:
```bash
# Python one-liner to strip spare bytes
python3 -c "
import sys
with open('nand_dump.bin','rb') as f, open('nand_data.bin','wb') as o:
    while True:
        page = f.read(2112)
        if len(page) < 2112: break
        o.write(page[:2048])
"
```

**Windows alternative (nand_final.exe):**

If native Linux USB does not work (e.g., when using usbipd to forward USB from Windows to WSL), a standalone Windows tool is provided:

```bash
# Cross-compile from Linux
make nand-win

# Or on Windows with MinGW:
# x86_64-w64-mingw32-gcc -O2 -o nand_final.exe tools/nand_final.c -lwinusb -lsetupapi
```

Run on Windows:
```
nand_final.exe output.bin
```

This uses the WinUSB driver directly and produces an identical dump.

### NAND Read Protocol Details

The NAND read protocol was reverse-engineered from Wireshark captures of the official Xgpro software:

1. Upload FPGA bitstream (same as for any chip)
2. Send **NAND_INIT** (cmd 0x02, 64 bytes) -- configures the NAND interface
3. Send **BEGIN_TRANS** (128 bytes, extended format) -- starts the NAND transaction
4. For each erase block (0 to 2047):
   - Send **READ_CODE** (0x0D, 16 bytes) with block counter
   - Receive **4 chunks of 33,792 bytes** on EP 0x82 (no header)
   - Each chunk = 16 pages x 2112 bytes
   - 4 chunks = 64 pages = 1 complete erase block
5. Send **END_TRANS** (0x04)

Total: 2048 commands x 135,168 bytes/command = 276,824,064 bytes

### Pin Contact Test (-z)

Before reading/writing, check if the chip is making good contact with the socket:

```bash
# Run pin contact test
sudo ./minipro-t76 -p "MT29F2G08ABDWP@TSOP48" -z

# With verbose output (shows raw pin bitmask)
sudo ./minipro-t76 -v -p "MT29F2G08ABDWP@TSOP48" -z
```

The test only checks **signal pins** -- NC (No Connect), VCC, and GND pins are skipped since they don't need signal contact. For example, a TSOP48 NAND chip has only ~15 signal pins out of 48 total.

Output shows each pin's role and status:
```
  Pin  Role     Status     Pin  Role     Status
    1  Signal    OK         25  NC       ---
    2  Signal    OK         26  Signal   *BAD*
    9  GND      ---         37  GND      ---
   23  VCC      ---         48  VCC      ---
```

### Verbose and Debug Modes

```bash
# -v   Verbose: shows chip parameters, bitstream loading, responses
sudo ./minipro-t76 -v -p "W25Q32 @SOIC8" -d

# -vv  Debug: full USB hex dumps of every packet (colored)
#      Shows all EP 0x01/0x81 traffic for protocol analysis
sudo ./minipro-t76 -vv -p "W25Q32 @SOIC8" -d 2>&1 | head -50

# Useful for comparing against Wireshark captures of the Windows software
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

Diagnostics:
  -z               Pin contact test (check for bad connections)
  -A <dir>         Use alternate algorithm directory (default: ./algoT76)

File format:
  -f <format>      Force file format: bin, ihex, srec (default: auto-detect)

Database:
  -l [filter]      List chips (supports glob patterns like W25Q* or substring search)
  -D <path>        Use alternate chip database file

Programmer:
  -i               Show programmer hardware/firmware info
  -v               Verbose output (stack: -v info, -vv hex dumps)
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

### FPGA Algorithm Files

The T76 has an FPGA that must be loaded with a bitstream before any chip operation. The algorithm files (`.alg`) are in the `algoT76/` directory, extracted from Xgpro. The tool loads them automatically.

If you see "algorithm directory not found", copy the `algoT76/` folder from the Xgpro installer:
```bash
# The algoT76 dir should be in the same directory as the minipro-t76 binary
ls algoT76/   # should show 362 .alg files
```

You can also specify a custom path:
```bash
sudo ./minipro-t76 -A /path/to/algoT76 -p "W25Q32 @SOIC8" -r dump.bin
```

---

## Chip Not in the Database? Alternative Methods

If your exact chip isn't in the database, **don't panic**. Most flash chips are pin-compatible clones of each other. Here's how to work around it.

### Method 1: Use an Equivalent Chip Entry

SPI NOR flash chips from different manufacturers are interchangeable if they have the same density and package. Pick any chip with matching size:

**32 Mbit (4 MB) SPI Flash — all interchangeable:**
| Your Chip | Use This Entry Instead |
|---|---|
| GD25Q32 (GigaDevice) | `W25Q32 @SOIC8` |
| MX25L3233F (Macronix) | `W25Q32 @SOIC8` |
| IS25LP032 (ISSI) | `W25Q32 @SOIC8` |
| EN25QH32 (EON) | `W25Q32 @SOIC8` |
| XM25QH32B (XMC) | `W25Q32 @SOIC8` |
| BY25Q32BS (BOYA) | `W25Q32 @SOIC8` |
| XT25F32B (XTX) | `W25Q32 @SOIC8` |
| ZB25VQ32 (Zetta) | `W25Q32 @SOIC8` |
| P25Q32H (Puya) | `W25Q32 @SOIC8` |
| FM25Q32A (Fudan Micro) | `W25Q32 @SOIC8` |

**Same pattern for other sizes:**
| Size | Use Entry |
|---|---|
| 8 Mbit (1 MB) | `W25Q80 @SOIC8` |
| 16 Mbit (2 MB) | `W25Q16 @SOIC8` |
| 64 Mbit (8 MB) | `W25Q64 @SOIC8` |
| 128 Mbit (16 MB) | `W25Q128 @SOIC8` |
| 256 Mbit (32 MB) | `W25Q256 @SOIC16` |

This works because all standard SPI flash chips use the same commands (0x03 read, 0x02 program, 0x20 erase, 0x9F JEDEC ID).

**Exception:** SST/Microchip chips (SST25VFxxx, SST26VFxxx) use a different protection scheme. Use the SST-specific entry if available, or issue a Global Block Unlock (0x98) first.

### Method 2: Identify by JEDEC ID

If you don't know what the chip is, read its JEDEC ID:

```bash
# Use any SPI flash entry to read the ID
sudo ./minipro-t76 -p "W25Q32 @SOIC8" -d
```

The 3 bytes tell you everything:
- **Byte 1** = Manufacturer
- **Byte 2** = Memory type
- **Byte 3** = Density

**Common manufacturer IDs:**
| ID | Manufacturer |
|---|---|
| `0xEF` | Winbond |
| `0xC8` | GigaDevice |
| `0xC2` | Macronix |
| `0x9D` | ISSI |
| `0xBF` | SST/Microchip |
| `0x01` | Spansion/Infineon |
| `0x20` | Micron (also XMC!) |
| `0x1C` | EON / cFeon |
| `0x1F` | Atmel / Microchip |
| `0x68` | BOYA |
| `0x0B` | XTX |
| `0xBA` | Zbit/Zetta |
| `0x85` | Puya |
| `0xA1` | Fudan Micro |
| `0x8C` | ESMT / Elite |

**Common density codes (byte 3):**
| Code | Size |
|---|---|
| `0x14` | 8 Mbit (1 MB) |
| `0x15` | 16 Mbit (2 MB) |
| `0x16` | 32 Mbit (4 MB) |
| `0x17` | 64 Mbit (8 MB) |
| `0x18` | 128 Mbit (16 MB) |
| `0x19` | 256 Mbit (32 MB) |

**Example:** ID `0xC84016` = GigaDevice (`0xC8`), 32 Mbit (`0x16`) → use `W25Q32 @SOIC8`

### Method 3: Check for Rebranded/Remarked Chips

Budget chips from AliExpress/eBay are often relabeled. Common scams:
- Chip marked "W25Q128" but actually 64 Mbit — verify with `-d`
- No-name chip with sanded-off markings — JEDEC ID still works
- "BOYA" or "XTX" branded chip — these are real chips, just use the Winbond equivalent entry of the same size

### Method 4: Unknown Parallel Flash (TSOP48)

For parallel NOR flash chips you can't identify:

1. **Check if it has CFI**: Most parallel NOR flash since ~2000 supports the Common Flash Interface standard, which means the programmer can query the chip's parameters automatically

2. **Try common equivalents by pin count**:
   - 48-pin TSOP: Try `MX29LV640EB @TSOP48` or `AM29LV640D @TSOP48`
   - The key is matching the bus width (x8 vs x16) and voltage (3.3V vs 5V)

3. **Read the ID first**: Use `-d` to read the manufacturer and device ID, then search the database:
   ```bash
   sudo ./minipro-t76 -p "MX29LV640EB @TSOP48" -d
   # If ID is 0x01227E: Spansion S29GL064, use that entry
   ```

### Method 5: NAND Flash

NAND flash (like your MT29F2G08) requires the correct algorithm loaded. The chip family determines which `.alg` file to use. If the exact part isn't in the database:

1. Find a chip from the same family:
   ```bash
   ./minipro-t76 -l "MT29F*"
   ./minipro-t76 -l "K9F*"     # Samsung NAND
   ```

2. NAND chips with the same page size (2048+64 or 4096+224) and bus width (x8 or x16) are usually interchangeable at the read level

3. The NAND ID structure: Byte 1 = Manufacturer, Byte 2 = Device code. Common NAND manufacturers:
   | ID | Manufacturer |
   |---|---|
   | `0x2C` | Micron |
   | `0xEC` | Samsung |
   | `0xAD` | SK Hynix |
   | `0x98` | Kioxia/Toshiba |
   | `0x01` | Spansion |
   | `0xC8` | GigaDevice |

### Quick Decision Tree

```
Can't find your chip in the database?
│
├─ Is it an 8-pin SPI flash?
│  └─ YES → Use W25Qxx @SOIC8 with matching size (check with -d)
│
├─ Is it a 16-pin SPI flash?
│  └─ YES → Use W25Qxx @SOIC16 or @SOP16 with matching size
│
├─ Is it a TSOP48 parallel flash?
│  └─ YES → Try MX29LVxxx or AM29Fxxx with matching size, read ID first with -d
│
├─ Is it a NAND flash?
│  └─ YES → Find same family in database (MT29F*, K9F*, etc.)
│
├─ Is it an EEPROM (8-pin, small)?
│  └─ YES → Use AT24Cxx or 93Cxx entry with matching size
│
└─ Still stuck?
   └─ Read the JEDEC ID with -d using any similar chip entry
      Then search: ./minipro-t76 -l "*" | grep <manufacturer_prefix>
```

---

## Windows WinUSB Driver

The T76 needs the WinUSB driver installed on Windows for proper USB 3.0 communication. A clean driver package is included in `driver/`:

```cmd
cd driver
install.bat       :: Run as Administrator to install
uninstall.bat     :: Run as Administrator to remove
```

This uses **only Microsoft's built-in WinUSB.sys** -- no custom binaries, no Chinese software. The driver is required for:
- The Windows `nand_final.exe` NAND reader
- Proper USB 3.0 SuperSpeed endpoint initialization
- Correct operation through `usbipd` (WSL USB forwarding)

See `driver/README.txt` for details.

---

## Project Structure

```
minipro-t76/
├── src/
│   ├── main.c           # CLI interface
│   ├── usb.c            # libusb transport (EP 0x01/0x81 cmd, EP 0x05/0x82 data)
│   ├── protocol.c       # Chip operations + NAND streaming protocol
│   ├── chipdb.c         # 65,178 chip database loader
│   ├── fileio.c         # Binary/Intel HEX/SREC I/O
│   ├── adapter.c        # Adapter image display
│   ├── algorithm.c      # FPGA bitstream loader (.alg files)
│   └── pintest.c        # Chip connectivity test
├── include/t76.h        # Protocol constants and API
├── chipdb.txt           # Full chip database (from minipro infoic.xml)
├── tools/
│   ├── nand_final.c     # Windows native NAND reader (WinUSB API)
│   ├── nand_scan2.c     # Quick NAND content scanner
│   ├── nand_addr_test.c # NAND address format discovery tool
│   ├── convert_infoic.py# Convert minipro infoic.xml to chipdb.txt
│   ├── extract_chipdb.c # Extract chip names from InfoICT76.dll
│   └── sniff_xgpro.py   # Wireshark USB capture decoder
├── driver/
│   ├── T76_WinUSB.inf   # Clean WinUSB driver (Microsoft WinUSB.sys)
│   ├── install.bat      # Driver installer
│   ├── uninstall.bat    # Driver uninstaller
│   └── README.txt       # Driver documentation
├── udev/
│   └── 60-minipro-t76.rules  # Linux udev rule for non-root access
├── Makefile
└── README.md
```

**Not included (must be extracted from Xgpro):**
- `algoT76/` -- 362 FPGA algorithm files (~66MB)
- `img/` -- 138 adapter setup images (~7MB)

---

## Protocol

Based on the verified USB protocol from the [minipro](https://gitlab.com/DavidGriffith/minipro) open-source project, with NAND protocol reverse-engineered from Wireshark captures of Xgpro.

| Endpoint | Direction | Purpose |
|----------|-----------|---------|
| 0x01 | OUT | Command messages |
| 0x81 | IN | Command responses |
| 0x05 | OUT | Bulk data write (T76-specific) |
| 0x82 | IN | Bulk data read (T76-specific) |

### NAND Read Protocol (reverse-engineered)

```
1. NAND_INIT (cmd 0x02, 64 bytes) -- configure NAND interface
2. BEGIN_TRANS (cmd 0x03, 128 bytes) -- extended format for NAND
3. READ_CODE (cmd 0x0D, 16 bytes per command):
   [0D 00] [block_LE16] [10 00 04 00 08 00 08 00 69 01 00 00]
4. Each command → 4 chunks of 33,792 bytes on EP 0x82
5. 33,792 = 16 pages × 2,112 bytes (2,048 data + 64 spare)
6. 4 chunks = 64 pages = 1 NAND erase block (135,168 bytes)
7. 2,048 commands for entire MT29F2G08 (264MB in ~15 seconds)
```

## Hardware

- **MCU:** WCH CH569 (RISC-V, USB 3.0 SuperSpeed)
- **FPGA:** Anlogic EG4X20BG256
- **USB:** VID `0xA466`, PID `0x1A86`
- **ZIF:** 48-pin with shift-register pin drivers

## Credits

- Protocol: [minipro project](https://gitlab.com/DavidGriffith/minipro) by David Griffith and radiomanV
- T76 hardware docs: [radiomanV/Xgecu_T76](https://github.com/radiomanV/Xgecu_T76)
- Chip database: Converted from minipro's `infoic.xml`
- NAND protocol: Reverse-engineered from Wireshark USB captures of Xgpro V13.09

## License

GPL-3.0 (same as minipro)
