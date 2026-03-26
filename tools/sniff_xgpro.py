#!/usr/bin/env python3
"""
sniff_xgpro.py - Parse USB captures from Wireshark/USBPcap

Decodes USB bulk transfers to/from the XGecu T76 programmer.
Use this to reverse-engineer the command protocol by capturing
traffic from the Windows Xgpro software.

Usage:
    1. Capture USB traffic in Wireshark (filter: usb.idVendor == 0xa466)
    2. Export as JSON: File -> Export Packet Dissections -> As JSON
    3. Run: python3 sniff_xgpro.py capture.json

Or use with tshark:
    tshark -r capture.pcapng -T json -Y "usb.transfer_type == 0x03" > capture.json
    python3 sniff_xgpro.py capture.json
"""

import json
import sys
import struct

# Known command bytes (update as we discover more)
COMMANDS = {
    0x00: "GET_DEVICE_INFO",
    0x01: "GET_DEVICE_STATUS",
    0x03: "BEGIN_TRANSACTION",
    0x04: "END_TRANSACTION",
    0x05: "SET_VCC",
    0x06: "SET_VPP",
    0x07: "SET_GND",
    0x08: "SET_IO",
    0x10: "READ_CHIP_ID",
    0x20: "READ_CODE",
    0x21: "READ_DATA",
    0x22: "READ_CONFIG",
    0x23: "READ_OTP",
    0x30: "WRITE_CODE",
    0x31: "WRITE_DATA",
    0x32: "WRITE_CONFIG",
    0x33: "WRITE_OTP",
    0x40: "ERASE_CHIP",
    0x41: "BLANK_CHECK",
    0x50: "VERIFY",
    0x60: "READ_PROTECT",
    0x61: "SET_PROTECT",
    0x62: "UNPROTECT",
    0x70: "SELFTEST_VCC",
    0x71: "SELFTEST_VPP",
    0x72: "SELFTEST_GND",
    0x73: "SELFTEST_IO",
    0xF0: "BOOTLOADER_ENTER",
    0xF1: "FIRMWARE_WRITE",
}

STATUS = {
    0x00: "OK",
    0x01: "ERROR",
    0x02: "BUSY",
    0x03: "NOT_BLANK",
    0x04: "VERIFY_FAIL",
    0x05: "OVERCURRENT",
}


def hex_dump(data, prefix="  "):
    """Format data as hex dump with ASCII."""
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_part = " ".join(f"{b:02X}" for b in chunk)
        ascii_part = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        lines.append(f"{prefix}{i:04X}: {hex_part:<48s} {ascii_part}")
    return "\n".join(lines)


def decode_packet(data, direction):
    """Decode a USB packet."""
    if not data:
        return "  (empty)"

    lines = []
    if direction == "OUT":
        cmd = data[0]
        cmd_name = COMMANDS.get(cmd, f"UNKNOWN_0x{cmd:02X}")
        lines.append(f"  Command: {cmd_name} (0x{cmd:02X})")

        if len(data) >= 8:
            addr = struct.unpack_from("<I", bytes(data), 4)[0] if len(data) >= 8 else 0
            length = struct.unpack_from("<I", bytes(data), 8)[0] if len(data) >= 12 else 0
            if addr or length:
                lines.append(f"  Address: 0x{addr:08X}, Length: 0x{length:08X} ({length})")
    else:
        status = data[0]
        status_name = STATUS.get(status, f"UNKNOWN_0x{status:02X}")
        lines.append(f"  Status: {status_name} (0x{status:02X})")

    lines.append(hex_dump(data))
    return "\n".join(lines)


def parse_wireshark_json(path):
    """Parse Wireshark JSON export."""
    with open(path) as f:
        packets = json.load(f)

    print(f"Loaded {len(packets)} packets from {path}")
    print("=" * 70)

    for i, pkt in enumerate(packets):
        layers = pkt.get("_source", {}).get("layers", {})
        usb = layers.get("usb", {})

        # Get direction
        endpoint = usb.get("usb.endpoint_address", "")
        if isinstance(endpoint, dict):
            endpoint = endpoint.get("usb.endpoint_address.number", "")

        # Get data
        data_hex = layers.get("usb.capdata", "")
        if not data_hex:
            continue

        # Parse hex data
        if isinstance(data_hex, str):
            data = bytes.fromhex(data_hex.replace(":", "").replace(" ", ""))
        else:
            continue

        # Determine direction from endpoint
        ep_num = int(endpoint) if endpoint else 0
        direction = "IN" if ep_num & 0x80 else "OUT"

        timestamp = usb.get("usb.frame_timestamp", "")
        data_len = len(data)

        print(f"\nPacket #{i+1} [{direction}] {data_len} bytes")
        if timestamp:
            print(f"  Time: {timestamp}")
        print(decode_packet(data, direction))


def parse_raw_hex(path):
    """Parse a simple hex dump file (one packet per line)."""
    with open(path) as f:
        lines = f.readlines()

    pkt_num = 0
    for line in lines:
        line = line.strip()
        if not line or line.startswith("#"):
            continue

        # Detect direction marker
        direction = "OUT"
        if line.startswith("IN:") or line.startswith("<"):
            direction = "IN"
            line = line.split(":", 1)[-1].strip().lstrip("<").strip()
        elif line.startswith("OUT:") or line.startswith(">"):
            direction = "OUT"
            line = line.split(":", 1)[-1].strip().lstrip(">").strip()

        try:
            data = bytes.fromhex(line.replace(" ", "").replace(":", ""))
        except ValueError:
            continue

        pkt_num += 1
        print(f"\nPacket #{pkt_num} [{direction}] {len(data)} bytes")
        print(decode_packet(data, direction))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    path = sys.argv[1]

    # Detect format
    with open(path) as f:
        first = f.read(1)

    if first == "[" or first == "{":
        parse_wireshark_json(path)
    else:
        parse_raw_hex(path)
