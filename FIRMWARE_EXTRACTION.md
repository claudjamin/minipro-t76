# Firmware Extraction Guide

Step-by-step walkthrough of extracting firmware from a NAND flash chip using the XGecu T76 programmer and the minipro-t76 toolset.

## Target Device

- **Chip:** Micron MT29F2G08ABDWP-ET (2Gbit NAND, TSOP48 package)
- **Chip ID:** 0x2CAA (Manufacturer: Micron, Device: MT29F2G08)
- **Size:** 264MB (131,072 pages × 2,112 bytes per page)
- **Page layout:** 2,048 bytes data + 64 bytes spare/OOB per page
- **Erase block:** 64 pages = 135,168 bytes (132KB)
- **Total erase blocks:** 2,048

## Equipment

- XGecu T76 universal programmer
- TSOP48 adapter (SN-ADP-048-0.5)
- USB 3.0 cable
- Windows PC with WinUSB driver installed

## Step 1: Hardware Setup

1. Seat the NAND chip in the TSOP48 adapter with pin 1 aligned
2. Insert the adapter into the T76's 48-pin ZIF socket
3. Close the ZIF lever firmly
4. Connect T76 to PC via USB 3.0

Verify chip is detected:
```cmd
cd C:\path\to\minipro-t76
tools\nand_final.exe
```

Expected output:
```
Chip: 2C AA 80 15 50 (Micron)
```

If chip ID shows `00 00 00 00 00`: reseat the adapter and ensure the WinUSB driver is installed (`driver\install.bat`).

## Step 2: Dump the NAND

```cmd
tools\nand_final.exe firmware.bin
```

This reads the entire chip in ~15 seconds at ~17 MB/s. The dump includes both data and spare/OOB areas.

Output:
```
Reading 2048 erase blocks (2048 × 135168 bytes)...
[########################################] 100% 264MB 17.6MB/s ETA:0s
Written: 276824064 bytes (264MB)
Errors: 0
```

## Step 3: Identify Firmware Structure

```bash
binwalk firmware.bin
```

Output:
```
DECIMAL       HEXADECIMAL     DESCRIPTION
314529        0x4CCA1         Certificate in DER format (x509 v3)
317064        0x4D688         CRC32 polynomial table, little endian
675840        0xA5000         UBI erase count header, version: 1, EC: 0x151
```

**Layout:**
| Offset | Size | Content |
|--------|------|---------|
| 0x00000 | ~660KB | U-Boot bootloader (ARM, version 2009.06-rc1) |
| 0xA5000 | ~263MB | UBI filesystem (contains Linux kernel + rootfs + apps) |

## Step 4: Strip OOB/Spare Data

The raw NAND dump has 2,112 bytes per page (2,048 data + 64 spare). UBI tools expect data-only pages. Strip the OOB:

```python
#!/usr/bin/env python3
"""strip_oob.py - Remove spare/OOB bytes from NAND dump"""

import sys

infile = sys.argv[1] if len(sys.argv) > 1 else 'firmware.bin'
outfile = sys.argv[2] if len(sys.argv) > 2 else 'firmware_no_oob.bin'

PAGE_TOTAL = 2112  # data + spare
PAGE_DATA = 2048   # data only

with open(infile, 'rb') as fin, open(outfile, 'wb') as fout:
    pages = 0
    while True:
        page = fin.read(PAGE_TOTAL)
        if len(page) < PAGE_TOTAL:
            break
        fout.write(page[:PAGE_DATA])
        pages += 1

print(f'Stripped OOB from {pages} pages')
print(f'Output: {pages * PAGE_DATA} bytes ({pages * PAGE_DATA // 1024 // 1024} MB)')
```

```bash
python3 strip_oob.py firmware.bin firmware_no_oob.bin
# Output: 268435456 bytes (256 MB)
```

## Step 5: Extract UBI Image

The UBI filesystem starts at offset 0xA5000 in the raw dump (with OOB). After stripping OOB, it's at offset 0xA0000 (655,360 bytes):

```bash
# Extract UBI portion (skip bootloader)
dd if=firmware_no_oob.bin of=ubi_clean.bin bs=4096 skip=160

# Verify UBI magic at PEB boundaries
python3 -c "
data = open('ubi_clean.bin','rb').read()
for i in range(5):
    off = i * 131072
    print(f'PEB {i}: {data[off:off+4]}')
"
# Should show: b'UBI#' for each PEB
```

## Step 6: Extract UBI Volumes

```bash
pip install ubi-reader python-lzo
ubireader_extract_images -o ubi_volumes ubi_clean.bin
```

This produces 7 UBI volumes (dual-boot system):

| Volume | Size | Description |
|--------|------|-------------|
| `img-0_vol-kernel1.ubifs` | 2.2 MB | Linux kernel (primary) - Linux 3.10.100 ARM |
| `img-0_vol-kernel2.ubifs` | 2.2 MB | Linux kernel (backup) |
| `img-0_vol-rootfs1.ubifs` | 28 MB | Root filesystem (primary) |
| `img-0_vol-rootfs2.ubifs` | 28 MB | Root filesystem (backup) |
| `img-0_vol-apps1.ubifs` | 37 MB | Application partition (primary) |
| `img-0_vol-apps2.ubifs` | 37 MB | Application partition (backup) |
| `img-0_vol-transfer.ubifs` | 29 MB | Data/transfer partition |

## Step 7: Extract Filesystem Contents

### Method A: Mount on Linux (requires kernel UBI/UBIFS support)

This requires a real Linux system (not WSL) with `ubi` and `ubifs` kernel modules:

```bash
# Load kernel modules
sudo modprobe ubi
sudo modprobe ubifs

# Create a virtual MTD device from the UBI image
# For MT29F2G08: page=2048, oob=64, pages_per_block=64
sudo modprobe nandsim first_id_byte=0x2c second_id_byte=0xaa \
    third_id_byte=0x80 fourth_id_byte=0x15

# Find the MTD device
cat /proc/mtd

# Write the UBI image to the virtual MTD
sudo flash_erase /dev/mtd0 0 0
sudo nandwrite -p /dev/mtd0 ubi_clean.bin

# Attach UBI
sudo ubiattach /dev/ubi_ctrl -m 0

# List volumes
sudo ubinfo -a

# Mount each volume
sudo mkdir -p /mnt/rootfs /mnt/apps /mnt/kernel
sudo mount -t ubifs ubi0:rootfs1 /mnt/rootfs
sudo mount -t ubifs ubi0:apps1 /mnt/apps

# Browse the filesystem
ls -la /mnt/rootfs/
find /mnt/rootfs -type f | head -50
```

### Method B: Extract with ubireader (no kernel modules needed)

```bash
ubireader_extract_files -o rootfs_extracted \
    ubi_volumes/ubi_clean.bin/img-0_vol-rootfs1.ubifs
```

Note: `ubireader_extract_files` may fail with "LEB: 191, Node size smaller than expected" on some UBIFS images due to LZO compression edge cases. If this happens, try:

```bash
# Upgrade to latest version
pip install --upgrade ubi-reader

# Or try with ignore errors (partial extraction)
ubireader_extract_files -i -o rootfs_extracted \
    ubi_volumes/ubi_clean.bin/img-0_vol-rootfs1.ubifs
```

### Method C: Use unblob (handles many formats)

```bash
pip install unblob
unblob --extract-dir rootfs_extracted \
    ubi_volumes/ubi_clean.bin/img-0_vol-rootfs1.ubifs
```

### Method D: Extract strings and data directly (always works)

Even if filesystem extraction fails, you can extract useful information directly:

```bash
# Find all file paths
strings img-0_vol-rootfs1.ubifs | grep -E '^/(bin|sbin|etc|usr|lib|var|home|root)/' | sort -u

# Extract password hashes
strings img-0_vol-rootfs1.ubifs | grep -E '^\$1\$|^root:|^admin:'

# Find config files
strings img-0_vol-rootfs1.ubifs | grep -iE '\.(conf|cfg|xml|json|key|pem|crt)$' | sort -u

# Find network information
strings img-0_vol-rootfs1.ubifs | grep -oE '[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+' | sort -u

# Find credentials
strings img-0_vol-rootfs1.ubifs | grep -iE '(password|passwd|secret|token|auth|login|admin|root:)'

# Find URLs and hostnames
strings img-0_vol-rootfs1.ubifs | grep -iE '(http://|https://|ftp://|ssh://)'

# Extract kernel version
strings img-0_vol-kernel1.ubifs | grep -i 'linux.*version\|linux-[0-9]'
```

## Step 8: Analyze Extracted Data

### Kernel Analysis
```bash
# Extract the kernel image (uImage format)
binwalk -e img-0_vol-kernel1.ubifs

# The kernel is a uImage with:
# - Linux 3.10.100
# - ARM architecture
# - Built 2019-02-05
# - No compression (raw zImage inside)
```

### Credential Extraction
```bash
# From the rootfs, we found:
# /etc/shadow equivalent:
echo 'root:$1$FCMJXCRe$xRH0tNmOG1hJ5z0JnA9x20' > hashes.txt
echo 'admin:$1$.WC3zV9w$C8XuNtT4Ly6ZlprdSTvTv1' >> hashes.txt

# Crack with hashcat (MD5-crypt = mode 500)
hashcat -m 500 hashes.txt /usr/share/wordlists/rockyou.txt

# Results (cracked in <1 second):
# root:pass
# admin:(empty password)
```

### Bootloader Analysis
```bash
# Extract U-Boot bootloader (first 660KB before UBI)
dd if=firmware_no_oob.bin of=bootloader.bin bs=1 count=655360

strings bootloader.bin | grep -i 'u-boot\|bootdelay\|bootargs\|bootcmd'
# U-Boot 2009.06-rc1 (Feb 05 2019)
# Contains boot arguments, memory layout, etc.
```

## Summary of Findings

| Item | Value |
|------|-------|
| **Bootloader** | U-Boot 2009.06-rc1 (Feb 2019) |
| **Kernel** | Linux 3.10.100 ARM |
| **Filesystem** | UBIFS on UBI (dual-boot A/B) |
| **Web server** | lighttpd |
| **Root password** | `pass` (MD5-crypt) |
| **Admin password** | *(empty)* (MD5-crypt) |
| **Network** | 172.25.44.x subnet |
| **SSL** | dh2048.pem, public.key, server certs |
| **Device type** | HID/ASSA ABLOY access control (DSA signatures present) |

## Tools Used

| Tool | Purpose |
|------|---------|
| `nand_final.exe` | Windows native NAND reader (WinUSB API) |
| `minipro-t76` | Linux CLI programmer (libusb) |
| `binwalk` | Firmware structure analysis |
| `ubireader_extract_images` | UBI volume extraction |
| `ubireader_extract_files` | UBIFS filesystem extraction |
| `strings` / `grep` | Raw data mining |
| `hashcat` | Password hash cracking |
| `python3` | OOB stripping, custom analysis |

## Protocol Details

The NAND read protocol was reverse-engineered from Wireshark captures of the official Xgpro software:

1. Upload FPGA bitstream (`T7_Nand_84.alg`)
2. Send NAND_INIT command (0x02, 64 bytes)
3. Send extended BEGIN_TRANS (0x03, 128 bytes)
4. For each erase block (0 to 2047):
   - Send READ_CODE: `0D 00 [block_LE16] 10 00 04 00 08 00 08 00 69 01 00 00`
   - Read 4 × 33,792 bytes from EP 0x82 (= 64 pages = 1 erase block)
5. Send END_TRANS (0x04)

Total: 2,048 commands × 4 reads × 33,792 bytes = 276,824,064 bytes in ~15 seconds.
