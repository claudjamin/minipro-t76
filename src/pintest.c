/*
 * minipro-t76 - Pin contact detection
 *
 * Tests if all chip pins are making good contact with the ZIF socket
 * or adapter. Equivalent to the "Pin Detection" feature in Xgpro.
 *
 * The T76 firmware drives each pin and checks for expected responses.
 * Pins that don't respond correctly indicate bad contact.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "t76.h"

/*
 * Run pin detection test.
 *
 * Protocol: Send T76_PIN_DETECTION (0x3E), receive 32-byte response.
 * The response contains a bitmask of pin states. Each bit that's 0
 * indicates a pin with bad or no contact.
 *
 * Must be called AFTER begin_transaction (chip must be configured).
 */
int t76_pin_test(t76_handle_t *dev, chip_t *chip)
{
    uint8_t msg[64] = { 0 };

    /* Get pin count from package_details */
    int pin_count = (chip->package_details >> 24) & 0xFF;
    if (pin_count == 0) {
        /* Try to extract from chip name */
        const char *at = strchr(chip->name, '@');
        if (at) {
            const char *p = at;
            while (*p && !(*p >= '0' && *p <= '9')) p++;
            if (*p) pin_count = atoi(p);
        }
    }
    if (pin_count == 0)
        pin_count = 48; /* default for TSOP48 */

    printf("Running pin contact test (%d pins)...\n", pin_count);

    /* Send pin detection command */
    msg[0] = T76_PIN_DETECTION;
    if (t76_msg_send(dev, msg, 8))
        return -1;

    memset(msg, 0, sizeof(msg));
    if (t76_msg_recv(dev, msg, sizeof(msg)))
        return -1;

    if (t76_verbose) {
        fprintf(stderr, "Pin detection response (first 16 bytes): ");
        for (int i = 0; i < 16; i++)
            fprintf(stderr, "%02X ", msg[i]);
        fprintf(stderr, "\n");
    }

    /* End the transaction after pin test (per reference code) */
    t76_end_transaction(dev);

    /*
     * Parse the response. The response contains pin state data.
     * Format: each byte contains status for 8 pins (bitmask).
     * A 0 bit indicates bad/no contact, 1 indicates good contact.
     *
     * The exact mapping depends on the chip's pin_map field.
     * For a basic check, we examine all bits up to pin_count.
     */
    int bad_pins = 0;
    int total_checked = 0;

    /* The pin data starts at msg[0] or msg[2] depending on firmware.
     * Check the first few bytes for the bitmask. */
    printf("\nPin Contact Results:\n");
    printf("  Pin  Status    Pin  Status\n");
    printf("  ---  ------    ---  ------\n");

    for (int pin = 1; pin <= pin_count; pin++) {
        /* Pins are mapped into the 32-byte response as a bitmask.
         * ZIF pin N maps to: byte[(N-1)/8], bit[(N-1)%8]
         * For the T76's 48-pin ZIF, this uses bytes 0-5. */
        int byte_idx = (pin - 1) / 8;
        int bit_idx = (pin - 1) % 8;

        if (byte_idx >= 32)
            break;

        int is_good = (msg[byte_idx] >> bit_idx) & 1;
        total_checked++;

        /* Print in two columns */
        if (pin <= pin_count / 2) {
            int right_pin = pin + pin_count / 2;
            int r_byte = (right_pin - 1) / 8;
            int r_bit = (right_pin - 1) % 8;
            int r_good = (r_byte < 32) ? ((msg[r_byte] >> r_bit) & 1) : 1;

            printf("  %3d  %s    %3d  %s\n",
                   pin, is_good ? "  OK  " : "* BAD *",
                   right_pin, r_good ? "  OK  " : "* BAD *");

            if (!is_good) bad_pins++;
            if (!r_good) bad_pins++;
            total_checked++;
        }
    }

    printf("\n");
    if (bad_pins == 0) {
        printf("Pin test PASSED - all %d pins have good contact.\n", pin_count);
    } else if (bad_pins == pin_count || bad_pins > pin_count / 2) {
        printf("Pin test: %d/%d pins show bad contact.\n", bad_pins, pin_count);
        printf("  -> Chip may not be inserted, or adapter is not connected.\n");
        printf("  -> Check that the ZIF lever is closed.\n");
    } else {
        printf("Pin test FAILED - %d pin(s) have bad contact:\n", bad_pins);
        printf("  -> Reseat the chip in the socket/adapter.\n");
        printf("  -> Clean the pins with isopropyl alcohol.\n");
        printf("  -> Check for bent or damaged pins.\n");
    }

    /* Also dump raw response for debugging */
    if (t76_verbose) {
        fprintf(stderr, "\nRaw pin detection bytes: ");
        for (int i = 0; i < 8; i++)
            fprintf(stderr, "%02X ", msg[i]);
        fprintf(stderr, "\n");

        /* Show as binary for each byte */
        for (int i = 0; i < (pin_count + 7) / 8 && i < 8; i++) {
            fprintf(stderr, "  Byte %d (pins %d-%d): ", i,
                    i * 8 + 1, (i + 1) * 8);
            for (int b = 7; b >= 0; b--)
                fprintf(stderr, "%d", (msg[i] >> b) & 1);
            fprintf(stderr, "\n");
        }
    }

    return bad_pins > 0 ? 1 : 0;
}
