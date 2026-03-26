#!/usr/bin/env python3
"""
convert_infoic.py - Convert minipro's infoic.xml to chipdb.txt

Extracts T76-compatible chip entries from the minipro project's
infoic.xml database and writes our tab-separated chipdb.txt format
with full parameters (variant, voltages, flags, algorithm mappings).

Usage:
    python3 convert_infoic.py <infoic.xml> [output.txt]

Get infoic.xml from: https://gitlab.com/DavidGriffith/minipro
"""

import xml.etree.ElementTree as ET
import sys

CHIP_TYPES = {
    "1": "MEMORY",
    "2": "MCU",
    "3": "PLD",
    "4": "SRAM",
    "5": "LOGIC",
    "6": "NAND",
    "7": "EMMC",
    "8": "VGA",
}

def parse_hex(s):
    """Parse a hex or decimal string to int."""
    if not s:
        return 0
    s = s.strip()
    if s.startswith("0x") or s.startswith("0X"):
        return int(s, 16)
    return int(s)

def convert(infoic_path, output_path=None):
    tree = ET.parse(infoic_path)
    root = tree.getroot()

    entries = []

    for db in root.findall("database"):
        db_type = db.get("type", "")
        # We want INFOICT76 database entries
        if db_type not in ("INFOICT76", "INFOIC2PLUS"):
            continue

        for mfg in list(db.findall("manufacturer")) + list(db.findall("custom")):
            for ic in mfg.findall("ic"):
                names = ic.get("name", "")
                if not names:
                    continue

                chip_type = ic.get("type", "0")
                protocol_id = parse_hex(ic.get("protocol_id", "0"))
                variant = parse_hex(ic.get("variant", "0"))
                read_buf = parse_hex(ic.get("read_buffer_size", "0"))
                write_buf = parse_hex(ic.get("write_buffer_size", "0"))
                code_size = parse_hex(ic.get("code_memory_size", "0"))
                data_size = parse_hex(ic.get("data_memory_size", "0"))
                data2_size = parse_hex(ic.get("data_memory2_size", "0"))
                page_size = parse_hex(ic.get("page_size", "0"))
                pages_per_block = parse_hex(ic.get("pages_per_block", "0"))
                chip_id = parse_hex(ic.get("chip_id", "0"))
                voltages = parse_hex(ic.get("voltages", "0"))
                pulse_delay = parse_hex(ic.get("pulse_delay", "0"))
                flags = parse_hex(ic.get("flags", "0"))
                chip_info = parse_hex(ic.get("chip_info", "0"))
                pin_map = parse_hex(ic.get("pin_map", "0"))
                pkg_details = parse_hex(ic.get("package_details", "0"))

                type_str = CHIP_TYPES.get(chip_type, "UNKNOWN")

                # Derive algorithm name from variant
                algo_hi = (variant >> 8) & 0xFF
                algo_name = ""
                if algo_hi:
                    if protocol_id == 0x03:  # SPI
                        algo_name = "SPI25F%02X" % algo_hi
                    elif chip_type == "6":  # NAND
                        algo_name = "Nand_%02X" % algo_hi
                    else:
                        algo_name = "%02X" % algo_hi

                # The name field can contain multiple comma-separated names
                for name in names.split(","):
                    name = name.strip()
                    if not name:
                        continue

                    # Output format: tab-separated
                    # name type chip_id code_size data_size page_size
                    # read_buf write_buf protocol_id variant voltages
                    # pulse_delay flags chip_info pin_map pkg_details
                    # algo_name pages_per_block data2_size
                    line = "\t".join([
                        name,
                        type_str,
                        "%x" % chip_id,
                        str(code_size),
                        str(data_size),
                        str(page_size),
                        str(read_buf),
                        str(write_buf),
                        "%x" % protocol_id,
                        "%x" % variant,
                        "%x" % voltages,
                        str(pulse_delay),
                        "%x" % flags,
                        "%x" % chip_info,
                        "%x" % pin_map,
                        "%x" % pkg_details,
                        algo_name,
                        str(pages_per_block),
                        str(data2_size),
                    ])
                    entries.append(line)

    # Write output
    out = sys.stdout
    if output_path:
        out = open(output_path, "w")

    out.write("# minipro-t76 chip database\n")
    out.write("# Converted from minipro infoic.xml (https://gitlab.com/DavidGriffith/minipro)\n")
    out.write("# %d chip entries\n" % len(entries))
    out.write("#\n")
    out.write("# Format: name\\ttype\\tchip_id\\tcode_size\\tdata_size\\tpage_size\\t"
              "read_buf\\twrite_buf\\tprotocol_id\\tvariant\\tvoltages\\t"
              "pulse_delay\\tflags\\tchip_info\\tpin_map\\tpkg_details\\t"
              "algo_name\\tpages_per_block\\tdata2_size\n")
    out.write("#\n")

    for line in entries:
        out.write(line + "\n")

    if output_path:
        out.close()
        print("Written %d entries to %s" % (len(entries), output_path))
    else:
        print("# Written %d entries" % len(entries), file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: %s <infoic.xml> [output.txt]" % sys.argv[0])
        sys.exit(1)

    convert(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
