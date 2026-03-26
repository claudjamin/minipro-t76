/*
 * minipro-t76 - Chip database
 *
 * Loads chip definitions from chipdb.txt (converted from minipro's infoic.xml).
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fnmatch.h>
#include "t76.h"

#define MAX_CHIPS 70000
#define MAX_LINE  2048
#define MAX_CODE_SIZE (256 * 1024 * 1024)

static chip_t *chips = NULL;
static int chip_count = 0;

static const char *chip_type_str(uint32_t type)
{
    switch (type) {
    case MP_MEMORY: return "MEMORY";
    case MP_MCU:    return "MCU";
    case MP_PLD:    return "PLD";
    case MP_SRAM:   return "SRAM";
    case MP_LOGIC:  return "LOGIC";
    case MP_NAND:   return "NAND";
    case MP_EMMC:   return "EMMC";
    case MP_VGA:    return "VGA";
    default:        return "UNKNOWN";
    }
}

static uint32_t parse_chip_type(const char *s)
{
    if (strcmp(s, "MEMORY") == 0 || strcmp(s, "SPI_FLASH") == 0 ||
        strcmp(s, "FLASH") == 0 || strcmp(s, "EEPROM") == 0 ||
        strcmp(s, "EPROM") == 0 || strcmp(s, "SERIAL_EEPROM") == 0)
        return MP_MEMORY;
    if (strcmp(s, "MCU") == 0) return MP_MCU;
    if (strcmp(s, "PLD") == 0) return MP_PLD;
    if (strcmp(s, "SRAM") == 0) return MP_SRAM;
    if (strcmp(s, "LOGIC") == 0) return MP_LOGIC;
    if (strcmp(s, "NAND") == 0) return MP_NAND;
    if (strcmp(s, "EMMC") == 0) return MP_EMMC;
    if (strcmp(s, "VGA") == 0) return MP_VGA;
    return 0;
}

int chipdb_load(const char *path)
{
    FILE *f;
    char line[MAX_LINE];

    if (chips) {
        free(chips);
        chips = NULL;
        chip_count = 0;
    }

    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: cannot open chip database '%s'\n", path);
        return -1;
    }

    chips = calloc(MAX_CHIPS, sizeof(chip_t));
    if (!chips) {
        fclose(f);
        return -1;
    }

    while (fgets(line, sizeof(line), f) && chip_count < MAX_CHIPS) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        nl = strchr(line, '\r');
        if (nl) *nl = '\0';

        if (line[0] == '#' || line[0] == '\0')
            continue;

        chip_t *c = &chips[chip_count];
        char type_str[32] = {0};
        char algo_str[16] = {0};
        unsigned int code_size = 0, data_size = 0, page_size = 0;
        unsigned int read_buf = 0, write_buf = 0;
        unsigned int protocol_id = 0, variant = 0, voltages = 0;
        unsigned int pulse_delay = 0, flags = 0, chip_info = 0;
        unsigned int pin_map = 0, pkg_details = 0;
        unsigned int pages_per_block = 0, data2_size = 0;

        /* New format: 19 tab-separated fields */
        int n = sscanf(line,
            "%39[^\t]\t%31[^\t]\t%x\t%u\t%u\t%u\t%u\t%u\t%x\t%x\t%x\t%u\t%x\t%x\t%x\t%x\t%15[^\t]\t%u\t%u",
            c->name, type_str, &c->chip_id,
            &code_size, &data_size, &page_size,
            &read_buf, &write_buf,
            &protocol_id, &variant, &voltages,
            &pulse_delay, &flags, &chip_info,
            &pin_map, &pkg_details,
            algo_str, &pages_per_block, &data2_size);

        if (n < 3) {
            /* Try old format (fewer fields) */
            memset(c, 0, sizeof(*c));
            unsigned int pins = 0, vcc = 0, vpp = 0;
            n = sscanf(line,
                "%39[^\t]\t%31[^\t]\t%x\t%u\t%*u\t%u\t%u\t%u\t%u\t%*u\t%u\t%u",
                c->name, type_str, &c->chip_id,
                &code_size, &page_size, &read_buf, &write_buf,
                &pins, &vcc, &vpp);
            if (n < 3)
                continue;
            pkg_details = (pins & 0xFF) << 24;
        }

        c->chip_type = parse_chip_type(type_str);
        c->code_memory_size = (code_size <= MAX_CODE_SIZE) ? code_size : 0;
        c->data_memory_size = data_size;
        c->data_memory2_size = data2_size;
        c->page_size = page_size;
        c->pages_per_block = pages_per_block;
        c->read_buffer_size = read_buf ? read_buf : 256;
        c->write_buffer_size = write_buf ? write_buf : 256;
        c->protocol_id = protocol_id;
        c->variant = variant;
        c->voltages_raw = voltages;
        c->pulse_delay = pulse_delay;
        c->flags_raw = flags;
        c->chip_info = chip_info;
        c->pin_map = pin_map;
        c->package_details = pkg_details;

        if (algo_str[0] && strcmp(algo_str, "0") != 0)
            strncpy(c->algo_name, algo_str, sizeof(c->algo_name) - 1);

        /* Derive chip_id_bytes_count from chip_id */
        if (c->chip_id) {
            if (c->chip_id > 0xFFFF) c->chip_id_bytes_count = 3;
            else if (c->chip_id > 0xFF) c->chip_id_bytes_count = 2;
            else c->chip_id_bytes_count = 1;
        }

        chip_count++;
    }

    fclose(f);
    printf("Loaded %d chips from database.\n", chip_count);
    return 0;
}

void chipdb_free(void)
{
    free(chips);
    chips = NULL;
    chip_count = 0;
}

chip_t *chipdb_find(const char *name)
{
    /* Exact match first */
    for (int i = 0; i < chip_count; i++) {
        if (strcasecmp(chips[i].name, name) == 0)
            return &chips[i];
    }

    /* Prefix match (chip name without package) */
    for (int i = 0; i < chip_count; i++) {
        size_t qlen = strlen(name);
        if (strncasecmp(chips[i].name, name, qlen) == 0) {
            char next = chips[i].name[qlen];
            if (next == '\0' || next == ' ' || next == '@')
                return &chips[i];
        }
    }

    return NULL;
}

void chipdb_list(const char *filter)
{
    int shown = 0;

    for (int i = 0; i < chip_count; i++) {
        int match = 1;

        if (filter && filter[0]) {
            if (strchr(filter, '*') || strchr(filter, '?')) {
                match = (fnmatch(filter, chips[i].name, FNM_CASEFOLD) == 0);
            } else {
                char name_lower[64], filter_lower[64];
                strncpy(name_lower, chips[i].name, sizeof(name_lower) - 1);
                name_lower[63] = '\0';
                strncpy(filter_lower, filter, sizeof(filter_lower) - 1);
                filter_lower[63] = '\0';
                for (char *p = name_lower; *p; p++) *p = tolower(*p);
                for (char *p = filter_lower; *p; p++) *p = tolower(*p);
                match = (strstr(name_lower, filter_lower) != NULL);
            }
        }

        if (match) {
            printf("%-40s  %-8s", chips[i].name, chip_type_str(chips[i].chip_type));
            if (chips[i].code_memory_size >= 1024)
                printf("  %6u KB", chips[i].code_memory_size / 1024);
            else if (chips[i].code_memory_size)
                printf("  %6u B ", chips[i].code_memory_size);
            if (chips[i].chip_id)
                printf("  ID:%06X", chips[i].chip_id);
            if (chips[i].algo_name[0])
                printf("  [%s]", chips[i].algo_name);
            printf("\n");
            shown++;
        }
    }

    printf("\n%d chip%s%s\n", shown, shown == 1 ? "" : "s",
           filter ? " matching" : " total");
}

int chipdb_count(void)
{
    return chip_count;
}
