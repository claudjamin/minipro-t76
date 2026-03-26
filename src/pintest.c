/*
 * minipro-t76 - Chip connectivity test
 *
 * Tests if the chip is properly connected by attempting to read its
 * JEDEC ID and comparing against the expected value. More reliable
 * than the T76_PIN_DETECTION command which appears to not return
 * actual pin state data on the T76 hardware.
 *
 * Also reports the raw pin detection firmware response for diagnostics.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "t76.h"

/*
 * Run connectivity test.
 *
 * 1. Sends T76_PIN_DETECTION for firmware-level check
 * 2. Re-begins transaction and reads chip ID
 * 3. Compares against expected ID from database
 *
 * This is a practical "does the chip work" test rather than
 * a per-pin electrical test.
 */
int t76_pin_test(t76_handle_t *dev, chip_t *chip)
{
    uint8_t msg[64] = { 0 };
    int pin_count = (chip->package_details >> 24) & 0xFF;
    if (pin_count == 0) pin_count = 48;

    printf("Running chip connectivity test (%d-pin %s)...\n\n",
           pin_count, chip->chip_type == MP_NAND ? "NAND" :
           chip->chip_type == MP_MEMORY ? "Flash" : "device");

    /* Step 1: Firmware pin detection (informational) */
    msg[0] = T76_PIN_DETECTION;
    if (t76_msg_send(dev, msg, 8) == 0) {
        memset(msg, 0, sizeof(msg));
        if (t76_msg_recv(dev, msg, sizeof(msg)) == 0) {
            if (t76_verbose) {
                fprintf(stderr, "Firmware pin detection response: ");
                for (int i = 0; i < 8; i++)
                    fprintf(stderr, "%02X ", msg[i]);
                fprintf(stderr, "\n");
            }
            printf("  Firmware pin check: %s (status=0x%02X)\n",
                   msg[1] == 0 ? "OK" : "ERROR", msg[1]);
        }
    }

    /* Pin detection ends the transaction, need to re-begin */
    t76_end_transaction(dev);

    /* Step 2: Re-begin transaction and read chip ID */
    printf("  Reading chip ID...\n");
    if (t76_begin_transaction(dev, chip) != 0) {
        printf("\n  ERROR: Cannot start transaction.\n");
        printf("  -> Check adapter connection and ZIF lever.\n");
        return -1;
    }

    uint8_t id_type;
    uint32_t chip_id = 0;
    int id_ok = (t76_get_chip_id(dev, chip, &id_type, &chip_id) == 0);

    t76_end_transaction(dev);

    /* Step 3: Analyze results */
    uint32_t expected_id = chip->chip_id;

    printf("  Expected chip ID: 0x%06X\n", expected_id);
    printf("  Read chip ID:     0x%06X\n", chip_id);

    if (!id_ok || chip_id == 0x000000) {
        printf("\n  RESULT: NO RESPONSE from chip\n");
        printf("\n  Possible causes:\n");
        printf("    1. Chip not inserted in adapter\n");
        printf("    2. Adapter not seated in ZIF socket\n");
        printf("    3. ZIF lever not closed\n");
        printf("    4. Wrong adapter for this package\n");
        printf("    5. Chip is dead or damaged\n");
        printf("\n  Try:\n");
        printf("    - Check adapter setup: ./minipro-t76 -p \"%s\" -a\n", chip->name);
        printf("    - Reseat adapter and close ZIF lever firmly\n");
        printf("    - Clean adapter pins with isopropyl alcohol\n");
        printf("    - Try a different chip to rule out adapter issues\n");
        return 1;
    }

    if (chip_id == 0xFFFFFF || chip_id == 0xFFFF) {
        printf("\n  RESULT: Chip returns all 1s (0x%06X)\n", chip_id);
        printf("\n  Possible causes:\n");
        printf("    1. Data pins not making contact\n");
        printf("    2. Chip select (CE#) not connected\n");
        printf("    3. Chip is blank/erased and returns 0xFF on ID read\n");
        printf("       (unusual but possible on some devices)\n");
        return 1;
    }

    /* Compare IDs - mask to the number of ID bytes */
    uint32_t mask = 0;
    for (int i = 0; i < chip->chip_id_bytes_count && i < 4; i++)
        mask |= (0xFF << (i * 8));
    if (!mask) mask = 0xFFFF;

    if ((chip_id & mask) == (expected_id & mask)) {
        printf("\n  RESULT: PASS - Chip ID matches! Connection is good.\n");
        return 0;
    }

    /* ID doesn't match but is non-zero -- partial connection? */
    printf("\n  RESULT: PARTIAL - Got a response but ID doesn't match.\n");
    printf("\n  This could mean:\n");
    printf("    1. Wrong chip selected (-p). Try: ./minipro-t76 -l \"*%02X*\"\n",
           chip_id & 0xFF);
    printf("    2. Some data pins have bad contact\n");
    printf("    3. The chip is a compatible part with a different ID\n");

    /* Try to identify by manufacturer byte */
    uint8_t mfg = chip_id & 0xFF;
    const char *mfg_name = "Unknown";
    if (mfg == 0x2C) mfg_name = "Micron";
    else if (mfg == 0xEC) mfg_name = "Samsung";
    else if (mfg == 0xAD) mfg_name = "SK Hynix";
    else if (mfg == 0x98) mfg_name = "Kioxia/Toshiba";
    else if (mfg == 0xEF) mfg_name = "Winbond";
    else if (mfg == 0xC8) mfg_name = "GigaDevice";
    else if (mfg == 0xC2) mfg_name = "Macronix";
    else if (mfg == 0x01) mfg_name = "Spansion/Infineon";
    else if (mfg == 0x20) mfg_name = "Micron/XMC";
    else if (mfg == 0x9D) mfg_name = "ISSI";
    else if (mfg == 0xBF) mfg_name = "SST/Microchip";
    else if (mfg == 0x1C) mfg_name = "EON";

    printf("    Read manufacturer: 0x%02X (%s)\n", mfg, mfg_name);
    printf("    Expected: 0x%02X\n", expected_id & 0xFF);

    return 1;
}
