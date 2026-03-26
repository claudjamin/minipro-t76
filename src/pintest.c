/*
 * minipro-t76 - Pin contact detection
 *
 * Tests if chip pins that need connection are making good contact.
 * Skips NC (No Connect), GND, and VCC pins that don't require
 * signal contact for the chip to work.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "t76.h"

/*
 * Pin role definitions.
 * NC pins and power pins don't need signal contact testing.
 */
#define PIN_SIGNAL  0   /* Active signal pin - must have contact */
#define PIN_NC      1   /* No Connect - skip test */
#define PIN_VCC     2   /* Power supply - skip test */
#define PIN_GND     3   /* Ground - skip test */
#define PIN_UNUSED  4   /* Not used in this configuration */

/*
 * TSOP48 NAND Flash pinout (standard Micron/Samsung/Hynix/Toshiba)
 * Based on MT29F series datasheet, applies to most x8 NAND
 *
 * Pin assignments (active-low signals marked with #):
 *  1: R/B#    2: RE#     3: CE#     4: NC     5: NC      6: NC
 *  7: NC      8: NC      9: GND    10: GND   11: IO0    12: IO1
 * 13: IO2    14: IO3    15: IO4    16: IO5   17: IO6    18: IO7
 * 19: NC     20: NC     21: NC     22: NC    23: VCC    24: NC
 * 25: NC     26: WE#    27: ALE    28: CLE   29: NC     30: WP#
 * 31: NC     32: NC     33: NC     34: NC    35: NC     36: NC
 * 37: GND    38: GND    39: NC     40: NC    41: NC     42: NC
 * 43: NC     44: NC     45: NC     46: NC    47: NC     48: VCC
 *
 * Note: Exact pinout varies by manufacturer and part number.
 * This is for standard x8 TSOP48 NAND. Some pins labeled NC may
 * be used for x16 bus width variants.
 */
static const uint8_t tsop48_nand_x8_pins[48] = {
    /* Pin  1-8  */ PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_NC,     PIN_NC,     PIN_NC,     PIN_NC,     PIN_NC,
    /* Pin  9-16 */ PIN_GND,    PIN_GND,    PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL,
    /* Pin 17-24 */ PIN_SIGNAL, PIN_SIGNAL, PIN_NC,     PIN_NC,     PIN_NC,     PIN_NC,     PIN_VCC,    PIN_NC,
    /* Pin 25-32 */ PIN_NC,     PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_NC,     PIN_SIGNAL, PIN_NC,     PIN_NC,
    /* Pin 33-40 */ PIN_NC,     PIN_NC,     PIN_NC,     PIN_NC,     PIN_GND,    PIN_GND,    PIN_NC,     PIN_NC,
    /* Pin 41-48 */ PIN_NC,     PIN_NC,     PIN_NC,     PIN_NC,     PIN_NC,     PIN_NC,     PIN_NC,     PIN_VCC,
};

/* TSOP48 NOR Flash (standard parallel NOR like MX29LV640, AM29F) */
static const uint8_t tsop48_nor_pins[48] = {
    /* Most NOR flash TSOP48 pins are active (address + data bus) */
    /* Pin  1-8  */ PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL,
    /* Pin  9-16 */ PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL,
    /* Pin 17-24 */ PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_GND,
    /* Pin 25-32 */ PIN_VCC,    PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL,
    /* Pin 33-40 */ PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL,
    /* Pin 41-48 */ PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL,
};

/* SPI Flash 8-pin (SOIC8/WSON8/DIP8) */
static const uint8_t spi8_pins[8] = {
    /* Pin 1-8 */ PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_GND, PIN_SIGNAL, PIN_SIGNAL, PIN_SIGNAL, PIN_VCC,
};

/* Generic: all pins are signal (conservative - flags everything) */
static const uint8_t all_signal_48[48] = { 0 };
static const uint8_t all_signal_8[8] = { 0 };

static const char *pin_role_str(uint8_t role)
{
    switch (role) {
    case PIN_NC:     return "NC";
    case PIN_VCC:    return "VCC";
    case PIN_GND:    return "GND";
    case PIN_UNUSED: return "N/A";
    default:         return NULL;
    }
}

/*
 * Get the pin map for a chip based on its type and package.
 * Returns pin_count and fills pin_roles array.
 */
static int get_pin_map(chip_t *chip, const uint8_t **pin_roles, int *pin_count)
{
    /* Get pin count from package_details */
    int pins = (chip->package_details >> 24) & 0xFF;
    if (pins == 0) {
        const char *at = strchr(chip->name, '@');
        if (at) {
            const char *p = at;
            while (*p && !(*p >= '0' && *p <= '9')) p++;
            if (*p) pins = atoi(p);
        }
    }
    if (pins == 0) pins = 48;
    *pin_count = pins;

    /* Match package and chip type to the right pin map */
    const char *name = chip->name;

    if (pins == 48 && chip->chip_type == MP_NAND) {
        *pin_roles = tsop48_nand_x8_pins;
        return 0;
    }

    if (pins == 48 && chip->chip_type == MP_MEMORY) {
        /* Check if it's a NOR flash */
        if (strstr(name, "29") || strstr(name, "SST39") ||
            strstr(name, "SST49") || strstr(name, "MX29") ||
            strstr(name, "AM29") || strstr(name, "S29")) {
            *pin_roles = tsop48_nor_pins;
            return 0;
        }
        *pin_roles = tsop48_nor_pins; /* default for 48-pin memory */
        return 0;
    }

    if (pins == 8) {
        *pin_roles = spi8_pins;
        return 0;
    }

    /* Fallback: treat all pins as signal (conservative) */
    if (pins <= 8)
        *pin_roles = all_signal_8;
    else
        *pin_roles = all_signal_48;
    return 0;
}

/*
 * Run pin detection test.
 */
int t76_pin_test(t76_handle_t *dev, chip_t *chip)
{
    uint8_t msg[64] = { 0 };
    const uint8_t *pin_roles;
    int pin_count;

    get_pin_map(chip, &pin_roles, &pin_count);

    printf("Running pin contact test (%d pins)...\n", pin_count);

    /* Send pin detection command */
    msg[0] = T76_PIN_DETECTION;
    if (t76_msg_send(dev, msg, 8))
        return -1;

    memset(msg, 0, sizeof(msg));
    if (t76_msg_recv(dev, msg, sizeof(msg)))
        return -1;

    if (t76_verbose) {
        fprintf(stderr, "Pin detection raw response: ");
        for (int i = 0; i < 8; i++)
            fprintf(stderr, "%02X ", msg[i]);
        fprintf(stderr, "\n");
    }

    /* End the transaction after pin test (per reference code) */
    t76_end_transaction(dev);

    /* Parse results */
    int bad_pins = 0;
    int tested_pins = 0;
    int bad_list[48];
    int bad_count = 0;

    printf("\n");
    printf("  Pin  Role     Status     Pin  Role     Status\n");
    printf("  ---  ----     ------     ---  ----     ------\n");

    int half = pin_count / 2;
    for (int row = 0; row < half; row++) {
        int left_pin = row + 1;
        int right_pin = row + half + 1;

        for (int side = 0; side < 2; side++) {
            int pin = (side == 0) ? left_pin : right_pin;
            int byte_idx = (pin - 1) / 8;
            int bit_idx = (pin - 1) % 8;
            uint8_t role = (pin <= pin_count && (pin - 1) < 48) ? pin_roles[pin - 1] : PIN_NC;
            const char *role_name = pin_role_str(role);

            if (side == 0)
                printf("  %3d  ", pin);
            else
                printf("     %3d  ", pin);

            if (role != PIN_SIGNAL) {
                /* NC/VCC/GND - don't test, just show role */
                printf("%-8s ---   ", role_name);
            } else {
                int is_good = (byte_idx < 32) ? ((msg[byte_idx] >> bit_idx) & 1) : 0;
                tested_pins++;

                if (is_good) {
                    printf("Signal    OK   ");
                } else {
                    printf("Signal   *BAD* ");
                    bad_pins++;
                    if (bad_count < 48)
                        bad_list[bad_count++] = pin;
                }
            }
        }
        printf("\n");
    }

    printf("\n");
    printf("Tested %d signal pins out of %d total pins.\n", tested_pins, pin_count);

    if (bad_pins == 0) {
        printf("Pin test PASSED - all signal pins have good contact.\n");
    } else if (bad_pins >= tested_pins) {
        printf("Pin test: ALL %d signal pins show bad contact.\n", bad_pins);
        printf("  -> Chip may not be inserted, or adapter is not connected.\n");
        printf("  -> Check that the ZIF lever is closed.\n");
    } else {
        printf("Pin test FAILED - %d signal pin(s) have bad contact:\n", bad_pins);
        printf("  Bad pins:");
        for (int i = 0; i < bad_count; i++)
            printf(" %d", bad_list[i]);
        printf("\n");
        printf("  -> Reseat the chip in the socket/adapter.\n");
        printf("  -> Clean the pins with isopropyl alcohol.\n");
        printf("  -> Check for bent or damaged pins.\n");
    }

    if (t76_verbose) {
        fprintf(stderr, "\nRaw pin bytes (binary):\n");
        for (int i = 0; i < (pin_count + 7) / 8 && i < 8; i++) {
            fprintf(stderr, "  Byte %d (pins %2d-%2d): ", i,
                    i * 8 + 1, i * 8 + 8 > pin_count ? pin_count : i * 8 + 8);
            for (int b = 7; b >= 0; b--)
                fprintf(stderr, "%d", (msg[i] >> b) & 1);
            fprintf(stderr, " (0x%02X)\n", msg[i]);
        }
    }

    return bad_pins > 0 ? 1 : 0;
}
