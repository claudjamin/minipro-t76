# minipro-t76

Open source Linux CLI programmer for the **XGecu T76** universal programmer.

Drop-in replacement for the Windows-only Xgpro software — no phone-home, no telemetry, no beaconing to `xgecu.com`. Just direct USB communication via libusb.

## Features

- **Read/Write/Erase/Verify** chips via command line
- **23,800+ chip database** extracted from Xgpro V13.17
- **Adapter setup images** — shows which adapter to use and how to place the chip, just like the Windows GUI (`-a` flag)
- **Intel HEX, SREC, and binary** file format support
- **SPI flash autodetect**
- **No network access** — zero connections to the internet, ever

## Protocol

Based on the verified USB protocol from the [minipro](https://gitlab.com/DavidGriffith/minipro) open-source project and binary analysis of Xgpro_T76.exe. The T76 uses the same command set as TL866II+/T48/T56 with T76-specific endpoint routing:

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

## Build

```bash
sudo apt install build-essential libusb-1.0-0-dev
make
```

## Install

```bash
sudo make install    # binary to /usr/local/bin
sudo make udev       # udev rule for non-root USB access
```

## Usage

```bash
# Show programmer info
minipro-t76 -i

# List chips
minipro-t76 -l W25Q*
minipro-t76 -l "AT29*"

# Show adapter setup image for a chip
minipro-t76 -p "W25Q32 @SOIC8" -a

# Read chip
minipro-t76 -p "W25Q32 @SOIC8" -r dump.bin

# Erase and write
minipro-t76 -p W25Q32 -e -w firmware.bin

# Detect chip ID
minipro-t76 -p W25Q32 -d

# Verify
minipro-t76 -p W25Q32 -m firmware.bin
```

## Adapter Images

Run with `-a` to see how to set up the programmer for your chip:

```bash
minipro-t76 -p "W25Q128 @SOIC8" -a    # shows SPI flash clip setup
minipro-t76 -p "M29W800AT @TSOP48" -a  # shows TSOP48 adapter placement
minipro-t76 -p "AT28C256 @DIP28" -a    # shows ZIF socket placement
```

Images are displayed using `xdg-open`, `feh`, `chafa` (terminal), or any available viewer.

## Credits

- Protocol reverse engineering: [minipro project](https://gitlab.com/DavidGriffith/minipro) by David Griffith and radiomanV
- T76 hardware documentation: [radiomanV/Xgecu_T76](https://github.com/radiomanV/Xgecu_T76)
- Chip database: Extracted from XGecu InfoICT76.dll

## License

GPL-3.0 (same as minipro, as this builds on their protocol work)
