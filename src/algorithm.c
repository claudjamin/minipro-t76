/*
 * minipro-t76 - FPGA algorithm/bitstream loader
 *
 * The T76 has an Anlogic FPGA that must be loaded with a bitstream
 * (algorithm) before any chip operations. Each chip family needs
 * a different bitstream that configures the FPGA pin mapping.
 *
 * Algorithm files are in algoT76/ directory, named like:
 *   T7_SPI25F11.alg   (SPI flash 8-pin)
 *   T7_SPI25F21.alg   (SPI flash 16-pin)
 *   T7_Nand_DA.alg    (NAND flash)
 *   T7_28F32P78.alg   (parallel NOR flash)
 *   etc.
 *
 * The algorithm number comes from the chip's variant field (high byte).
 * The algorithm name comes from the chip database.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "t76.h"

#define DEFAULT_ALGO_DIR  "/usr/share/minipro-t76/algoT76"
#define LOCAL_ALGO_DIR    "./algoT76"

/* Find the algorithm directory */
static const char *find_algo_dir(const char *override)
{
    struct stat st;

    if (override && stat(override, &st) == 0)
        return override;

    if (stat(LOCAL_ALGO_DIR, &st) == 0)
        return LOCAL_ALGO_DIR;

    if (stat(DEFAULT_ALGO_DIR, &st) == 0)
        return DEFAULT_ALGO_DIR;

    return NULL;
}

/*
 * T76 .alg file structure:
 *   Bytes 0-3:     Header/version
 *   Bytes 4-11:    8-byte bitstream metadata (size info + checksum)
 *   Bytes 0xC0+:   Description string (e.g. "NAND", "SPI25F11")
 *   Bytes 0x1000:  Padding (0xFF)
 *   Bytes 0x1001+: Actual FPGA bitstream data
 *
 * The data sent to the T76 is: 8 bytes from offset 4, then
 * everything from offset 4097 (0x1001) onwards.
 * This matches minipro's dump-alg-minipro.bash:
 *   (tail -c +5 $file | head -c 8 && tail -c +$T76_ALG_OFFSET $file)
 */
#define T76_ALG_HEADER_SIZE   4      /* skip first 4 bytes */
#define T76_ALG_META_SIZE     8      /* 8-byte metadata at offset 4 */
#define T76_ALG_DATA_OFFSET   4097   /* bitstream data starts here (0x1001) */

/*
 * Load a T76 algorithm file from disk.
 * Strips the header and assembles the bitstream as:
 *   [8 bytes from offset 4] + [data from offset 4097 to end]
 * Returns malloc'd buffer that caller must free.
 */
static uint8_t *load_algo_file(const char *path, size_t *length)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 2 * 1024 * 1024) { /* 2MB max */
        fclose(f);
        return NULL;
    }

    /* Read entire file first */
    uint8_t *raw = malloc(fsize);
    if (!raw) {
        fclose(f);
        return NULL;
    }

    size_t nread = fread(raw, 1, fsize, f);
    fclose(f);

    if ((long)nread != fsize) {
        free(raw);
        return NULL;
    }

    /* Check if file is large enough for the T76 format */
    if (fsize > T76_ALG_DATA_OFFSET) {
        /* T76 .alg format: assemble bitstream from header metadata + data */
        size_t data_len = fsize - T76_ALG_DATA_OFFSET;
        size_t total = T76_ALG_META_SIZE + data_len;

        uint8_t *bitstream = malloc(total);
        if (!bitstream) {
            free(raw);
            return NULL;
        }

        /* Copy 8-byte metadata from offset 4 */
        memcpy(bitstream, &raw[T76_ALG_HEADER_SIZE], T76_ALG_META_SIZE);
        /* Copy bitstream data from offset 4097 */
        memcpy(bitstream + T76_ALG_META_SIZE, &raw[T76_ALG_DATA_OFFSET], data_len);

        free(raw);
        *length = total;
        return bitstream;
    }

    /* Small file -- treat as raw bitstream (already processed) */
    *length = nread;
    return raw;
}

/*
 * Get the algorithm filename for a chip.
 *
 * The minipro project derives the filename from:
 *   "T7_" + algo_name + ".alg"
 * where algo_name is looked up from the variant field.
 *
 * For our standalone tool, we support:
 * 1. Explicit algo_name in chip_t (from enhanced chipdb)
 * 2. Fallback: try common algorithm names based on chip type
 */
int t76_load_algorithm(t76_handle_t *dev, chip_t *chip, const char *algo_dir_override)
{
    const char *algo_dir = find_algo_dir(algo_dir_override);
    if (!algo_dir) {
        fprintf(stderr, "Error: algorithm directory not found.\n"
                "Copy algoT76/ from Xgpro installer to ./ or %s\n",
                DEFAULT_ALGO_DIR);
        return -1;
    }

    /* If already uploaded in this session, skip */
    if (dev->bitstream_uploaded)
        return 0;

    /* Build algorithm filename */
    char algo_path[512];
    char algo_name[64] = { 0 };

    if (chip->algo_name[0]) {
        /* Use explicit algorithm name from chip database */
        snprintf(algo_name, sizeof(algo_name), "T7_%s", chip->algo_name);
    } else {
        /* Try to guess from chip type and variant */
        uint8_t algo_id = (chip->variant >> 8) & 0xFF;

        if (algo_id) {
            /* The algorithm ID maps to a hex suffix */
            snprintf(algo_name, sizeof(algo_name), "T7_SPI25F%02X", algo_id);
        } else if (chip->protocol_id == SPI_PROTOCOL) {
            /* Default SPI flash algorithms */
            const char *at = strchr(chip->name, '@');
            int pins = 8;
            if (at) {
                const char *p = at;
                while (*p && !(*p >= '0' && *p <= '9')) p++;
                if (*p) pins = atoi(p);
            }
            if (pins <= 8)
                snprintf(algo_name, sizeof(algo_name), "T7_SPI25F11");
            else
                snprintf(algo_name, sizeof(algo_name), "T7_SPI25F21");
        } else {
            fprintf(stderr, "Error: cannot determine algorithm for chip '%s'\n",
                    chip->name);
            fprintf(stderr, "The T76 FPGA needs a bitstream algorithm to operate.\n"
                    "Specify the algorithm directory with the algoT76/ files.\n");
            return -1;
        }
    }

    snprintf(algo_path, sizeof(algo_path), "%s/%s.alg", algo_dir, algo_name);

    /* Try the exact path first */
    struct stat st;
    if (stat(algo_path, &st) != 0) {
        /* Try some common fallbacks for SPI flash */
        const char *fallbacks[] = {
            "T7_SPI25F11", "T7_SPI25F21", "T7_SPI25F10", NULL
        };

        int found = 0;
        if (chip->protocol_id == SPI_PROTOCOL) {
            for (int i = 0; fallbacks[i]; i++) {
                snprintf(algo_path, sizeof(algo_path), "%s/%s.alg",
                         algo_dir, fallbacks[i]);
                if (stat(algo_path, &st) == 0) {
                    snprintf(algo_name, sizeof(algo_name), "%s", fallbacks[i]);
                    found = 1;
                    break;
                }
            }
        }

        if (!found) {
            fprintf(stderr, "Error: algorithm file not found: %s/%s.alg\n",
                    algo_dir, algo_name);
            fprintf(stderr, "Available algorithms in %s:\n", algo_dir);
            /* List a few matching algorithms */
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "ls %s/*.alg 2>/dev/null | head -10",
                     algo_dir);
            /* Don't use system() for security - just tell user to look */
            fprintf(stderr, "  Run: ls %s/*.alg\n", algo_dir);
            return -1;
        }
    }

    /* Load the algorithm file */
    size_t algo_len;
    uint8_t *bitstream = load_algo_file(algo_path, &algo_len);
    if (!bitstream) {
        fprintf(stderr, "Error: cannot read algorithm file: %s\n", algo_path);
        return -1;
    }

    fprintf(stderr, "Loading T76 algorithm: %s (%zu bytes)\n", algo_name, algo_len);

    /* Upload to the T76 FPGA */
    int ret = t76_write_bitstream(dev, bitstream, algo_len);
    free(bitstream);

    if (ret < 0) {
        fprintf(stderr, "Error: failed to upload bitstream to T76 FPGA\n");
        return -1;
    }

    dev->bitstream_uploaded = 1;
    return 0;
}
