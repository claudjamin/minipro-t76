/*
 * minipro-t76 - Open source CLI programmer for XGecu T76
 *
 * Usage:
 *   minipro-t76 -p <chip> -r <file>    Read chip to file
 *   minipro-t76 -p <chip> -w <file>    Write file to chip
 *   minipro-t76 -p <chip> -e           Erase chip
 *   minipro-t76 -p <chip> -d           Read chip ID / detect
 *   minipro-t76 -p <chip> -a           Show adapter/setup image
 *   minipro-t76 -l [filter]            List supported chips
 *   minipro-t76 -i                     Show programmer info
 *
 * No phone-home. No beaconing. No telemetry.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include "t76.h"

#define VERSION "0.2.0"
#define DEFAULT_CHIPDB_PATH "/usr/share/minipro-t76/chipdb.txt"
#define LOCAL_CHIPDB_PATH   "./chipdb.txt"

static t76_handle_t dev;
static int verbose = 0;

static void cleanup(void)
{
    t76_close(&dev);
}

static volatile sig_atomic_t interrupted = 0;

static void sighandler(int sig)
{
    (void)sig;
    interrupted = 1;
}

static void usage(const char *prog)
{
    printf("minipro-t76 v%s - Open source XGecu T76 programmer\n\n", VERSION);
    printf("Usage: %s [options]\n\n", prog);
    printf("Device operations:\n");
    printf("  -p <chip>        Select chip by name\n");
    printf("  -r <file>        Read chip to file\n");
    printf("  -w <file>        Write file to chip\n");
    printf("  -e               Erase chip\n");
    printf("  -m <file>        Verify chip against file\n");
    printf("  -d               Detect/read chip ID\n");
    printf("  -u               Unprotect before operation\n");
    printf("  -P               Protect after operation\n");
    printf("\n");
    printf("Adapter/Setup:\n");
    printf("  -a               Show adapter setup image for chip\n");
    printf("  -I <dir>         Image directory (default: ./img or /usr/share/minipro-t76/img)\n");
    printf("\n");
    printf("File format:\n");
    printf("  -f <format>      bin, ihex, srec (default: auto)\n");
    printf("\n");
    printf("Database:\n");
    printf("  -l [filter]      List chips (glob/substring filter)\n");
    printf("  -D <path>        Alternate chip database file\n");
    printf("\n");
    printf("Programmer:\n");
    printf("  -i               Show hardware/firmware info\n");
    printf("  -v               Verbose output\n");
    printf("  -h, --help       Show this help\n");
    printf("  -V, --version    Show version\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -p \"W25Q32 @SOIC8\" -r flash.bin\n", prog);
    printf("  %s -p W25Q32 -e -w firmware.bin\n", prog);
    printf("  %s -p \"W25Q32 @SOIC8\" -a        # show adapter setup\n", prog);
    printf("  %s -l W25*\n", prog);
    printf("  %s -i\n", prog);
}

static file_format_t parse_format(const char *s)
{
    if (strcasecmp(s, "bin") == 0 || strcasecmp(s, "raw") == 0) return FMT_BIN;
    if (strcasecmp(s, "ihex") == 0 || strcasecmp(s, "hex") == 0) return FMT_IHEX;
    if (strcasecmp(s, "srec") == 0 || strcasecmp(s, "s19") == 0) return FMT_SREC;
    return FMT_AUTO;
}

static const char *find_chipdb(const char *override)
{
    if (override) return override;
    FILE *f = fopen(LOCAL_CHIPDB_PATH, "r");
    if (f) { fclose(f); return LOCAL_CHIPDB_PATH; }
    return DEFAULT_CHIPDB_PATH;
}

int main(int argc, char **argv)
{
    char *chip_name = NULL;
    char *read_file_path = NULL;
    char *write_file_path = NULL;
    char *verify_file_path = NULL;
    char *chipdb_path = NULL;
    char *image_dir = NULL;
    char *list_filter = NULL;
    file_format_t file_fmt = FMT_AUTO;
    int do_erase = 0, do_detect = 0, do_info = 0;
    int do_list = 0, do_unprotect = 0, do_protect = 0;
    int do_adapter = 0;
    int ret;

    static struct option long_opts[] = {
        {"help",    no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "p:r:w:m:f:D:I:l::ediuPavhV",
                              long_opts, NULL)) != -1) {
        switch (opt) {
        case 'p': chip_name = optarg; break;
        case 'r': read_file_path = optarg; break;
        case 'w': write_file_path = optarg; break;
        case 'm': verify_file_path = optarg; break;
        case 'f': file_fmt = parse_format(optarg); break;
        case 'D': chipdb_path = optarg; break;
        case 'I': image_dir = optarg; break;
        case 'l': do_list = 1; list_filter = optarg; break;
        case 'e': do_erase = 1; break;
        case 'd': do_detect = 1; break;
        case 'i': do_info = 1; break;
        case 'u': do_unprotect = 1; break;
        case 'P': do_protect = 1; break;
        case 'a': do_adapter = 1; break;
        case 'v': verbose = 1; break;
        case 'V':
            printf("minipro-t76 v%s\n", VERSION);
            return 0;
        case 'h':
        default:
            usage(argv[0]);
            return (opt == 'h') ? 0 : 1;
        }
    }

    /* Handle -l filter from remaining args */
    if (do_list && !list_filter && optind < argc)
        list_filter = argv[optind];

    /* === List chips (no device needed) === */
    if (do_list) {
        if (chipdb_load(find_chipdb(chipdb_path)) < 0)
            return 1;
        chipdb_list(list_filter);
        chipdb_free();
        return 0;
    }

    /* === Show programmer info === */
    if (do_info) {
        ret = t76_open(&dev);
        if (ret < 0)
            return 1;
        t76_print_device_info(&dev);
        t76_close(&dev);
        return 0;
    }

    /* === Show adapter image (no device needed) === */
    if (do_adapter && chip_name) {
        if (chipdb_load(find_chipdb(chipdb_path)) < 0)
            return 1;
        chip_t *chip = chipdb_find(chip_name);
        if (!chip) {
            fprintf(stderr, "Error: chip '%s' not found\n", chip_name);
            chipdb_free();
            return 1;
        }
        ret = show_adapter_image(chip, image_dir);
        chipdb_free();
        return ret < 0 ? 1 : 0;
    }

    /* All remaining operations need a chip or detect mode */
    if (!chip_name && !do_detect) {
        if (!read_file_path && !write_file_path && !verify_file_path &&
            !do_erase) {
            usage(argv[0]);
            return 1;
        }
        fprintf(stderr, "Error: -p <chip> required\n");
        return 1;
    }

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    atexit(cleanup);

    /* Load chip database */
    chip_t *chip = NULL;
    if (chip_name) {
        if (chipdb_load(find_chipdb(chipdb_path)) < 0)
            return 1;

        chip = chipdb_find(chip_name);
        if (!chip) {
            fprintf(stderr, "Error: chip '%s' not found\n", chip_name);
            fprintf(stderr, "Try: -l '%s*'\n", chip_name);
            chipdb_free();
            return 1;
        }

        if (verbose)
            printf("Selected: %s (ID: %06X, %u bytes)\n",
                   chip->name, chip->chip_id, chip->code_memory_size);

        if (verbose) {
            const char *at = strchr(chip->name, '@');
            if (at)
                printf("Package: %s\n", at + 1);
        }
    }

    /* Open programmer */
    ret = t76_open(&dev);
    if (ret < 0) {
        chipdb_free();
        return 1;
    }

    if (verbose)
        t76_print_device_info(&dev);

    /* Detect chip ID */
    if (do_detect) {
        uint8_t type;
        uint32_t chip_id;

        if (chip) {
            ret = t76_begin_transaction(&dev, chip);
            if (ret < 0) goto fail;

            ret = t76_get_chip_id(&dev, chip, &type, &chip_id);
        } else {
            /* SPI autodetect without chip selection */
            ret = t76_spi_autodetect(&dev, SPI_DEVICE_8P, &chip_id);
            type = 0;
        }

        if (ret == 0) {
            printf("Chip ID: 0x%06X\n", chip_id);
            printf("  Manufacturer: 0x%02X\n", chip_id & 0xFF);
            printf("  Device: 0x%04X\n", (chip_id >> 8) & 0xFFFF);
        }

        if (chip) t76_end_transaction(&dev);

        if (!read_file_path && !write_file_path && !do_erase)
            goto done;
    }

    if (!chip)
        goto done;

    /* Begin transaction */
    ret = t76_begin_transaction(&dev, chip);
    if (ret < 0)
        goto fail;

    /* Unprotect */
    if (do_unprotect) {
        printf("Unprotecting...\n");
        t76_protect_off(&dev);
    }

    /* Erase */
    if (do_erase) {
        ret = t76_erase(&dev, chip);
        if (ret < 0)
            goto fail_end;
    }

    /* Write */
    if (write_file_path) {
        uint32_t buf_size = chip->code_memory_size;
        if (!buf_size) buf_size = 16 * 1024 * 1024;

        uint8_t *buf = calloc(1, buf_size);
        if (!buf) { fprintf(stderr, "Out of memory\n"); goto fail_end; }

        uint32_t data_len;
        ret = file_read_buf(write_file_path, file_fmt, buf, buf_size, &data_len);
        if (ret < 0) { free(buf); goto fail_end; }

        printf("Writing %u bytes from '%s'...\n", data_len, write_file_path);

        /* Declare t76_write_code_memory */
        extern int t76_write_code_memory(t76_handle_t *, chip_t *, uint8_t *, uint32_t);
        ret = t76_write_code_memory(&dev, chip, buf, data_len);
        if (ret < 0) { free(buf); goto fail_end; }

        /* Auto-verify */
        printf("Verifying...\n");
        uint8_t *vbuf = calloc(1, buf_size);
        if (!vbuf) { free(buf); goto fail_end; }

        extern int t76_read_code_memory(t76_handle_t *, chip_t *, uint8_t *);
        ret = t76_read_code_memory(&dev, chip, vbuf);
        if (ret == 0) {
            for (uint32_t i = 0; i < data_len; i++) {
                if (vbuf[i] != buf[i]) {
                    fprintf(stderr, "Verify FAILED at 0x%08X: "
                            "wrote 0x%02X, read 0x%02X\n",
                            i, buf[i], vbuf[i]);
                    ret = -1;
                    break;
                }
            }
            if (ret == 0)
                printf("Verify OK.\n");
        }
        free(vbuf);
        free(buf);
        if (ret < 0) goto fail_end;
    }

    /* Read */
    if (read_file_path) {
        uint32_t read_size = chip->code_memory_size;
        if (!read_size) {
            fprintf(stderr, "Error: unknown code memory size\n");
            goto fail_end;
        }

        uint8_t *buf = calloc(1, read_size);
        if (!buf) { fprintf(stderr, "Out of memory\n"); goto fail_end; }

        printf("Reading %u bytes...\n", read_size);
        extern int t76_read_code_memory(t76_handle_t *, chip_t *, uint8_t *);
        ret = t76_read_code_memory(&dev, chip, buf);
        if (ret < 0) { free(buf); goto fail_end; }

        file_format_t out_fmt = (file_fmt == FMT_AUTO) ? FMT_BIN : file_fmt;
        ret = file_write_buf(read_file_path, out_fmt, buf, read_size);
        free(buf);
        if (ret < 0) goto fail_end;

        printf("Saved to '%s'\n", read_file_path);
    }

    /* Verify (standalone) */
    if (verify_file_path) {
        uint32_t buf_size = chip->code_memory_size;
        if (!buf_size) buf_size = 16 * 1024 * 1024;

        uint8_t *fbuf = calloc(1, buf_size);
        uint8_t *rbuf = calloc(1, buf_size);
        if (!fbuf || !rbuf) {
            free(fbuf); free(rbuf);
            fprintf(stderr, "Out of memory\n");
            goto fail_end;
        }

        uint32_t data_len;
        ret = file_read_buf(verify_file_path, file_fmt, fbuf, buf_size, &data_len);
        if (ret < 0) { free(fbuf); free(rbuf); goto fail_end; }

        printf("Verifying against '%s' (%u bytes)...\n", verify_file_path, data_len);
        extern int t76_read_code_memory(t76_handle_t *, chip_t *, uint8_t *);
        ret = t76_read_code_memory(&dev, chip, rbuf);
        if (ret == 0) {
            for (uint32_t i = 0; i < data_len; i++) {
                if (rbuf[i] != fbuf[i]) {
                    fprintf(stderr, "Verify FAILED at 0x%08X: "
                            "expected 0x%02X, read 0x%02X\n",
                            i, fbuf[i], rbuf[i]);
                    ret = -1;
                    break;
                }
            }
            if (ret == 0)
                printf("Verify OK.\n");
        }
        free(fbuf);
        free(rbuf);
        if (ret < 0) goto fail_end;
    }

    /* Protect */
    if (do_protect) {
        printf("Protecting...\n");
        t76_protect_on(&dev);
    }

    t76_end_transaction(&dev);

done:
    t76_close(&dev);
    chipdb_free();
    return 0;

fail_end:
    t76_end_transaction(&dev);
fail:
    t76_close(&dev);
    chipdb_free();
    return 1;
}
