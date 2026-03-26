/*
 * extract_chipdb - Extract chip database from InfoICT76.dll
 *
 * This tool parses the InfoICT76.dll binary to extract chip names and
 * parameters. The DLL exports GetIcList/GetIcStru which return chip
 * data in structured format. Since we can't call Win32 DLL exports
 * directly on Linux, we parse the binary data sections.
 *
 * The chip entries in the DLL follow a structured format with:
 * - Chip name string (null-terminated)
 * - Fixed-size parameter block containing voltages, sizes, timing, etc.
 *
 * Usage: extract_chipdb <InfoICT76.dll> [output.txt]
 *
 * Strategy:
 * 1. Find chip name strings (identified by @ package suffix pattern)
 * 2. Look for the structured data arrays that reference these names
 * 3. Export as tab-separated chipdb.txt
 *
 * If full parameter extraction fails, falls back to name-only mode
 * which at least gives us a searchable chip list.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

/* Known package suffixes in the database */
static const char *packages[] = {
    "@SOIC8", "@SOIC16", "@SOP8", "@SOP16", "@SOP28", "@SOP32",
    "@SOP44", "@TSOP48", "@TSOP56", "@TSOP32", "@TSSOP8", "@TSSOP20",
    "@TSSOP28", "@QFN8", "@QFP44", "@QFP48", "@QFP64",
    "@DIP8", "@DIP14", "@DIP16", "@DIP18", "@DIP20", "@DIP24",
    "@DIP28", "@DIP32", "@DIP40", "@DIP42", "@DIP48",
    "@SDIP42", "@SDIP56",
    "@PLCC20", "@PLCC28", "@PLCC32", "@PLCC44", "@PLCC52", "@PLCC68",
    "@PLCC84",
    "@BGA48", "@BGA63", "@BGA100", "@BGA132", "@BGA152", "@BGA169",
    "@BGA256",
    "@WSON8", "@SSOP8", "@SSOP20", "@SSOP28",
    "@TQFP32", "@TQFP44", "@TQFP48", "@TQFP64", "@TQFP100",
    "@LQFP44", "@LQFP48", "@LQFP64", "@LQFP100",
    "@SOJ28", "@SOJ32", "@SOJ40", "@SOJ44",
    "@UFBGA48", "@FBGA63", "@FBGA48",
    "@CSP48",
    "@FPN8",
    "@LGA52",
    NULL
};

static int is_chip_name_char(unsigned char c)
{
    return isalnum(c) || c == '_' || c == '-' || c == '.' || c == '/'
        || c == '(' || c == ')' || c == '+' || c == ' ' || c == '@';
}

/* Check if a string at the given position looks like a chip name */
static int looks_like_chip_name(const uint8_t *data, size_t pos, size_t file_size)
{
    /* Must start with alphanumeric */
    if (!isalnum(data[pos]))
        return 0;

    /* Find end of string */
    size_t end = pos;
    while (end < file_size && data[end] != 0 && is_chip_name_char(data[end]))
        end++;

    if (end >= file_size || data[end] != 0)
        return 0;

    size_t len = end - pos;

    /* Reasonable length: 3 to 63 chars */
    if (len < 3 || len > 63)
        return 0;

    /* Check for @ package suffix */
    const char *str = (const char *)&data[pos];
    const char *at = strchr(str, '@');
    if (!at)
        return 0;

    /* Verify it's a known package */
    for (int i = 0; packages[i]; i++) {
        if (strstr(str, packages[i]) != NULL)
            return 1;
    }

    /* Even if package isn't in our list, if it has @XXX pattern it's likely valid */
    if (at[1] && isupper(at[1]))
        return 1;

    return 0;
}

/* Extract chip names from the DLL binary */
static int extract_names(const uint8_t *data, size_t file_size,
                         char names[][64], int max_names)
{
    int count = 0;

    for (size_t i = 0; i < file_size - 4 && count < max_names; i++) {
        if (looks_like_chip_name(data, i, file_size)) {
            const char *str = (const char *)&data[i];
            size_t len = strlen(str);

            /* Skip duplicates */
            int dup = 0;
            for (int j = 0; j < count; j++) {
                if (strcmp(names[j], str) == 0) {
                    dup = 1;
                    break;
                }
            }

            if (!dup) {
                strncpy(names[count], str, 63);
                names[count][63] = '\0';
                count++;
            }

            /* Skip past this string */
            i += len;
        }
    }

    return count;
}

/* Try to guess chip type from name */
static const char *guess_chip_type(const char *name)
{
    /* SPI NOR Flash */
    if (strncasecmp(name, "W25Q", 4) == 0 ||
        strncasecmp(name, "W25X", 4) == 0 ||
        strncasecmp(name, "MX25L", 5) == 0 ||
        strncasecmp(name, "MX25U", 5) == 0 ||
        strncasecmp(name, "MX25V", 5) == 0 ||
        strncasecmp(name, "GD25Q", 5) == 0 ||
        strncasecmp(name, "GD25B", 5) == 0 ||
        strncasecmp(name, "GD25LQ", 6) == 0 ||
        strncasecmp(name, "EN25Q", 5) == 0 ||
        strncasecmp(name, "EN25T", 5) == 0 ||
        strncasecmp(name, "IS25LP", 6) == 0 ||
        strncasecmp(name, "IS25WP", 6) == 0 ||
        strncasecmp(name, "SST25", 5) == 0 ||
        strncasecmp(name, "SST26", 5) == 0 ||
        strncasecmp(name, "AT25", 4) == 0 ||
        strncasecmp(name, "PM25", 4) == 0 ||
        strncasecmp(name, "XM25Q", 5) == 0 ||
        strncasecmp(name, "BY25Q", 5) == 0 ||
        strncasecmp(name, "FM25Q", 5) == 0 ||
        strncasecmp(name, "N25Q", 4) == 0 ||
        strncasecmp(name, "MT25Q", 5) == 0 ||
        strncasecmp(name, "S25FL", 5) == 0 ||
        strncasecmp(name, "M25P", 4) == 0 ||
        strncasecmp(name, "M25PE", 5) == 0)
        return "SPI_FLASH";

    /* SPI NAND */
    if (strncasecmp(name, "GD5F", 4) == 0 ||
        strncasecmp(name, "W25N", 4) == 0 ||
        strncasecmp(name, "MX35", 4) == 0 ||
        strncasecmp(name, "MT29F", 5) == 0)
        return "SPI_NAND";

    /* Parallel NOR Flash */
    if (strncasecmp(name, "AM29", 4) == 0 ||
        strncasecmp(name, "M29", 3) == 0 ||
        strncasecmp(name, "S29", 3) == 0 ||
        strncasecmp(name, "MX29", 4) == 0 ||
        strncasecmp(name, "SST39", 5) == 0 ||
        strncasecmp(name, "SST49", 5) == 0 ||
        strncasecmp(name, "AT29", 4) == 0 ||
        strncasecmp(name, "EN29", 4) == 0 ||
        strncasecmp(name, "A29", 3) == 0)
        return "FLASH";

    /* EPROM */
    if (strncasecmp(name, "27C", 3) == 0 ||
        strncasecmp(name, "27SF", 4) == 0 ||
        strncasecmp(name, "M27C", 4) == 0 ||
        strncasecmp(name, "AM27C", 5) == 0 ||
        strncasecmp(name, "NM27C", 5) == 0 ||
        strncasecmp(name, "TMS27", 5) == 0)
        return "EPROM";

    /* EEPROM (parallel) */
    if (strncasecmp(name, "28C", 3) == 0 ||
        strncasecmp(name, "AT28C", 5) == 0 ||
        strncasecmp(name, "X28C", 4) == 0 ||
        strncasecmp(name, "CAT28", 5) == 0)
        return "EEPROM";

    /* Serial EEPROM */
    if (strncasecmp(name, "24C", 3) == 0 ||
        strncasecmp(name, "24LC", 4) == 0 ||
        strncasecmp(name, "AT24C", 5) == 0 ||
        strncasecmp(name, "CAT24", 5) == 0 ||
        strncasecmp(name, "93C", 3) == 0 ||
        strncasecmp(name, "93LC", 4) == 0 ||
        strncasecmp(name, "AT93C", 5) == 0 ||
        strncasecmp(name, "25LC", 4) == 0 ||
        strncasecmp(name, "M95", 3) == 0 ||
        strncasecmp(name, "M24", 3) == 0 ||
        strncasecmp(name, "M93", 3) == 0)
        return "SERIAL_EEPROM";

    /* SRAM */
    if (strncasecmp(name, "HM62", 4) == 0 ||
        strncasecmp(name, "HY62", 4) == 0 ||
        strncasecmp(name, "CY62", 4) == 0 ||
        strncasecmp(name, "IS61", 4) == 0 ||
        strncasecmp(name, "IS62", 4) == 0 ||
        strncasecmp(name, "UM61", 4) == 0 ||
        strncasecmp(name, "BS62", 4) == 0 ||
        strncasecmp(name, "AS6C", 4) == 0 ||
        strncasecmp(name, "LY62", 4) == 0 ||
        strncasecmp(name, "IDT71", 5) == 0)
        return "SRAM";

    /* MCU */
    if (strncasecmp(name, "PIC", 3) == 0 ||
        strncasecmp(name, "ATmega", 6) == 0 ||
        strncasecmp(name, "ATtiny", 6) == 0 ||
        strncasecmp(name, "AT89", 4) == 0 ||
        strncasecmp(name, "AT90", 4) == 0 ||
        strncasecmp(name, "MSP430", 6) == 0 ||
        strncasecmp(name, "STM8", 4) == 0 ||
        strncasecmp(name, "STC", 3) == 0 ||
        strncasecmp(name, "dsPIC", 5) == 0)
        return "MCU";

    /* PLD */
    if (strncasecmp(name, "GAL", 3) == 0 ||
        strncasecmp(name, "PAL", 3) == 0 ||
        strncasecmp(name, "ATF16", 5) == 0 ||
        strncasecmp(name, "ATF22", 5) == 0 ||
        strncasecmp(name, "ATF750", 6) == 0 ||
        strncasecmp(name, "PALCE", 5) == 0)
        return "PLD";

    /* NAND */
    if (strncasecmp(name, "K9F", 3) == 0 ||
        strncasecmp(name, "K9K", 3) == 0 ||
        strncasecmp(name, "MT29", 4) == 0 ||
        strncasecmp(name, "TC58", 4) == 0 ||
        strncasecmp(name, "HY27", 4) == 0)
        return "NAND";

    /* eMMC */
    if (strncasecmp(name, "EMMC", 4) == 0 ||
        strncasecmp(name, "KLMAG", 5) == 0 ||
        strncasecmp(name, "THGBM", 5) == 0)
        return "EMMC";

    return "UNKNOWN";
}

/* Try to guess memory size from chip name (very rough heuristic) */
static uint32_t guess_code_size(const char *name)
{
    /* Extract numeric part after known prefix */
    const char *p = name;

    /* Skip manufacturer prefix to find size indicator */
    /* Common patterns: W25Q128 = 128Mbit = 16MB, W25Q32 = 32Mbit = 4MB */

    /* For SPI flash, number usually means megabits */
    if (strncasecmp(name, "W25Q", 4) == 0 || strncasecmp(name, "W25X", 4) == 0) {
        p = name + 4;
    } else if (strncasecmp(name, "MX25L", 5) == 0 || strncasecmp(name, "MX25U", 5) == 0) {
        p = name + 5;
    } else if (strncasecmp(name, "GD25Q", 5) == 0 || strncasecmp(name, "GD25B", 5) == 0) {
        p = name + 5;
    } else if (strncasecmp(name, "EN25Q", 5) == 0) {
        p = name + 5;
    } else if (strncasecmp(name, "SST25VF", 7) == 0) {
        p = name + 7;
    } else {
        return 0; /* Can't guess */
    }

    int size_mbit = atoi(p);
    if (size_mbit > 0)
        return (uint32_t)size_mbit * 1024 * 1024 / 8; /* Mbit to bytes */

    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <InfoICT76.dll> [output.txt]\n", argv[0]);
        fprintf(stderr, "\nExtracts chip database from the XGecu DLL.\n");
        return 1;
    }

    const char *dll_path = argv[1];
    const char *out_path = argc > 2 ? argv[2] : NULL;

    FILE *f = fopen(dll_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s'\n", dll_path);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *data = malloc(file_size);
    if (!data) {
        fclose(f);
        return 1;
    }

    fread(data, 1, file_size, f);
    fclose(f);

    printf("Loaded %s (%ld bytes)\n", dll_path, file_size);
    printf("Scanning for chip names...\n");

    /* Allocate space for chip names */
    #define MAX_CHIPS 32000
    static char names[MAX_CHIPS][64];
    int count = extract_names(data, file_size, names, MAX_CHIPS);

    printf("Found %d unique chip entries.\n", count);

    /* Output */
    FILE *out = stdout;
    if (out_path) {
        out = fopen(out_path, "w");
        if (!out) {
            fprintf(stderr, "Error: cannot open '%s' for writing\n", out_path);
            free(data);
            return 1;
        }
    }

    fprintf(out, "# minipro-t76 chip database\n");
    fprintf(out, "# Extracted from InfoICT76.dll (Xgpro T76 V13.17)\n");
    fprintf(out, "# %d chips\n", count);
    fprintf(out, "#\n");
    fprintf(out, "# Format: name<TAB>type<TAB>chip_id<TAB>code_size<TAB>data_size<TAB>"
            "page_size<TAB>read_block<TAB>write_block<TAB>pins<TAB>flags<TAB>"
            "vcc<TAB>vpp<TAB>pulse_delay<TAB>adapter<TAB>algo<TAB>protocol\n");
    fprintf(out, "#\n");
    fprintf(out, "# NOTE: Parameters beyond name and type are estimates.\n");
    fprintf(out, "# Full parameter extraction requires USB sniffing to map\n");
    fprintf(out, "# the DLL's internal structures. Use --sniff mode to help\n");
    fprintf(out, "# map the actual protocol for specific chips.\n");
    fprintf(out, "#\n");

    for (int i = 0; i < count; i++) {
        const char *type = guess_chip_type(names[i]);
        uint32_t code_size = guess_code_size(names[i]);

        /* Default SPI flash parameters */
        uint32_t page_size = 256;
        uint32_t read_block = 4096;
        uint32_t write_block = 256;
        uint16_t vcc = 3300;
        uint16_t vpp = 0;
        uint8_t pins = 8;

        if (strcmp(type, "FLASH") == 0) {
            page_size = 64;
            read_block = 64;
            write_block = 64;
            pins = 48;
            vcc = 3300;
        } else if (strcmp(type, "EPROM") == 0) {
            page_size = 1;
            read_block = 256;
            write_block = 1;
            vcc = 5000;
            vpp = 12500;
            pins = 28;
        } else if (strcmp(type, "EEPROM") == 0 || strcmp(type, "SERIAL_EEPROM") == 0) {
            page_size = 64;
            read_block = 64;
            write_block = 64;
            vcc = 5000;
            pins = 8;
        } else if (strcmp(type, "MCU") == 0) {
            page_size = 64;
            read_block = 256;
            write_block = 64;
            vcc = 5000;
            pins = 40;
        } else if (strcmp(type, "PLD") == 0) {
            page_size = 32;
            read_block = 32;
            write_block = 32;
            vcc = 5000;
            pins = 24;
        } else if (strcmp(type, "NAND") == 0) {
            page_size = 2048;
            read_block = 2048;
            write_block = 2048;
            pins = 48;
        }

        /* Guess pin count from package name */
        const char *at = strchr(names[i], '@');
        if (at) {
            /* Extract number from package suffix */
            const char *p = at + 1;
            while (*p && !isdigit(*p)) p++;
            if (*p) {
                int n = atoi(p);
                if (n > 0 && n <= 100)
                    pins = n;
            }
        }

        fprintf(out, "%s\t%s\t0\t%u\t0\t%u\t%u\t%u\t%u\t0\t%u\t%u\t0\t0\t0\t0\n",
                names[i], type, code_size, page_size,
                read_block, write_block, pins, vcc, vpp);
    }

    if (out_path) {
        fclose(out);
        printf("Written to '%s'\n", out_path);
    }

    free(data);
    return 0;
}
