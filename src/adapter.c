/*
 * minipro-t76 - Adapter and setup image display
 *
 * Shows chip placement diagrams and adapter setup images,
 * just like the Windows Xgpro GUI does.
 *
 * Uses xdg-open (Linux), open (macOS), or feh/display as fallback.
 * Images are stored as JPGs extracted from the Xgpro installer.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "t76.h"

#define DEFAULT_IMAGE_DIR  "/usr/share/minipro-t76/img"
#define LOCAL_IMAGE_DIR    "./img"

/* Adapter name -> image filename mapping for common adapters */
static const struct {
    const char *package;
    const char *image;
    const char *description;
} adapter_map[] = {
    /* SPI flash - 8-pin */
    { "@SOIC8",     "T76_25_WSON8.jpg",      "SPI Flash 8-pin SOIC/WSON - Place in ZIF socket or use SOIC8 clip" },
    { "@SOP8",      "T76_25_WSON8.jpg",      "SPI Flash 8-pin SOP - Place in ZIF or use SOP8 clip" },
    { "@WSON8",     "T76_WSON8_127EX.jpg",   "WSON8 adapter required (T76_WSON8_127EX)" },
    { "@QFN8",      "T76_25_WSON8.jpg",      "QFN8 - Use WSON8/QFN8 adapter" },
    { "@FPN8",      "T76_25_WSON8.jpg",      "FPN8 - Use WSON8/FPN8 adapter" },
    { "@TSSOP8",    "T76_25_WSON8.jpg",      "TSSOP8 - Use TSSOP8 adapter or clip" },

    /* SPI flash - 16-pin */
    { "@SOIC16",    "T76_SOP16_127EX.jpg",   "SPI Flash 16-pin SOIC - Use SOP16 adapter (T76_SOP16_127EX)" },
    { "@SOP16",     "T76_SOP16_127EX.jpg",   "SOP16 adapter (T76_SOP16_127EX)" },

    /* TSOP48 parallel flash */
    { "@TSOP48",    "T76B48B63.jpg",         "TSOP48 adapter required (T76_B48)" },

    /* TSOP56 */
    { "@TSOP56",    "T76P56.jpg",            "TSOP56 adapter required (T76_P56)" },

    /* BGA */
    { "@BGA24",     "T76BGA24_4x6.jpg",      "BGA24 adapter required - Check pin pitch (4x6 or 5x5)" },
    { "@BGA48",     "T76B48B63.jpg",          "BGA48 adapter required" },
    { "@BGA63",     "T76B48B63.jpg",          "BGA63 adapter required" },
    { "@BGA64",     "T76BGA64.jpg",           "BGA64 adapter required" },
    { "@BGA100",    "T76EMMCBGA.jpg",         "BGA100 adapter required" },
    { "@BGA132",    "T76EMMCBGA.jpg",         "BGA132 adapter required (eMMC)" },
    { "@BGA153",    "T76EMMCBGA.jpg",         "BGA153 adapter required (eMMC)" },
    { "@BGA169",    "T76EMMCBGA.jpg",         "BGA169 adapter required (eMMC)" },

    /* DIP - direct placement in ZIF */
    { "@DIP8",      "NoAdapter.jpg",          "DIP8 - Place directly in ZIF socket (pin 1 at top)" },
    { "@DIP14",     "NoAdapter.jpg",          "DIP14 - Place directly in ZIF socket (pin 1 at top)" },
    { "@DIP16",     "NoAdapter.jpg",          "DIP16 - Place directly in ZIF socket (pin 1 at top)" },
    { "@DIP18",     "NoAdapter.jpg",          "DIP18 - Place directly in ZIF socket (pin 1 at top)" },
    { "@DIP20",     "NoAdapter.jpg",          "DIP20 - Place directly in ZIF socket (pin 1 at top)" },
    { "@DIP24",     "NoAdapter.jpg",          "DIP24 - Place directly in ZIF socket (pin 1 at top)" },
    { "@DIP28",     "NoAdapter.jpg",          "DIP28 - Place directly in ZIF socket (pin 1 at top)" },
    { "@DIP32",     "NoAdapter.jpg",          "DIP32 - Place directly in ZIF socket (pin 1 at top)" },
    { "@DIP40",     "NoAdapter.jpg",          "DIP40 - Place directly in ZIF socket (pin 1 at top)" },
    { "@DIP42",     "T48_DIP42.jpg",          "DIP42 adapter or alignment required" },
    { "@DIP48",     "NoAdapter.jpg",          "DIP48 - Place directly in 48-pin ZIF socket" },

    /* PLCC */
    { "@PLCC20",    "T86PLCC32.jpg",          "PLCC adapter required" },
    { "@PLCC28",    "T86PLCC32.jpg",          "PLCC28 adapter required" },
    { "@PLCC32",    "T86PLCC32.jpg",          "PLCC32 adapter required" },
    { "@PLCC44",    "T86PLCC44.jpg",          "PLCC44 adapter required" },
    { "@PLCC52",    "T86PLCC44.jpg",          "PLCC52 adapter required" },

    /* TQFP */
    { "@TQFP32",    "T86TQFP32.jpg",         "TQFP32 adapter required" },
    { "@TQFP44",    "T86TQFP32.jpg",         "TQFP44 adapter required" },

    /* SOJ */
    { "@SOJ28",     "T76B48B63.jpg",          "SOJ adapter required" },
    { "@SOJ32",     "T76B48B63.jpg",          "SOJ adapter required" },
    { "@SOJ44",     "T76B48B63.jpg",          "SOJ44 adapter required" },

    /* SOP44 */
    { "@SOP44",     "T76B48B63.jpg",          "SOP44 adapter required" },

    /* eMMC/SD */
    { "@eMMC",      "T76_EMMC_Prog.jpg",      "eMMC BGA adapter required" },
    { "@SD",        "T76SD.jpg",               "SD card slot" },

    /* ISP/ICSP */
    { "ISP",        "T76ICP001.jpg",           "ISP header connection" },

    { NULL, NULL, NULL }
};

/* Find the image directory */
static const char *find_image_dir(const char *override)
{
    struct stat st;

    if (override && stat(override, &st) == 0)
        return override;

    if (stat(LOCAL_IMAGE_DIR, &st) == 0)
        return LOCAL_IMAGE_DIR;

    if (stat(DEFAULT_IMAGE_DIR, &st) == 0)
        return DEFAULT_IMAGE_DIR;

    return NULL;
}

/* Get image name for a chip based on its package */
const char *get_adapter_image_name(chip_t *chip)
{
    if (chip->adapter_image[0])
        return chip->adapter_image;

    /* Look up by package suffix */
    const char *at = strchr(chip->name, '@');
    if (!at)
        return "NoAdapter.jpg";

    for (int i = 0; adapter_map[i].package; i++) {
        if (strstr(at, adapter_map[i].package + 1) != NULL)
            return adapter_map[i].image;
    }

    return "NoAdapter.jpg";
}

/* Get the adapter description text */
static const char *get_adapter_description(chip_t *chip)
{
    const char *at = strchr(chip->name, '@');
    if (!at)
        return "Place chip directly in ZIF socket";

    for (int i = 0; adapter_map[i].package; i++) {
        if (strstr(at, adapter_map[i].package + 1) != NULL)
            return adapter_map[i].description;
    }

    return "Check adapter requirements for this package";
}

/* Try to open an image with available viewers */
static int open_image(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "Image not found: %s\n", path);
        return -1;
    }

    printf("Opening: %s\n", path);

    /* Try image viewers in order of preference */
    const char *viewers[] = {
        "xdg-open",     /* Standard Linux */
        "feh",          /* Lightweight image viewer */
        "eog",          /* GNOME Eye of GNOME */
        "display",      /* ImageMagick */
        "xv",           /* Classic X viewer */
        "open",         /* macOS */
        NULL
    };

    for (int i = 0; viewers[i]; i++) {
        char cmd[512];
        /* Check if the viewer exists */
        snprintf(cmd, sizeof(cmd), "which %s >/dev/null 2>&1", viewers[i]);
        if (system(cmd) == 0) {
            snprintf(cmd, sizeof(cmd), "%s '%s' &", viewers[i], path);
            return system(cmd);
        }
    }

    /* If no graphical viewer, try to display in terminal */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "which chafa >/dev/null 2>&1");
    if (system(cmd) == 0) {
        /* chafa renders images in the terminal */
        snprintf(cmd, sizeof(cmd), "chafa --size=80x40 '%s'", path);
        return system(cmd);
    }

    snprintf(cmd, sizeof(cmd), "which catimg >/dev/null 2>&1");
    if (system(cmd) == 0) {
        snprintf(cmd, sizeof(cmd), "catimg -w 80 '%s'", path);
        return system(cmd);
    }

    fprintf(stderr, "No image viewer found. Image is at: %s\n", path);
    fprintf(stderr, "Install one of: feh, eog, chafa, catimg, imagemagick\n");
    return -1;
}

/*
 * Show adapter/setup image for a chip
 */
int show_adapter_image(chip_t *chip, const char *image_dir_override)
{
    const char *image_dir = find_image_dir(image_dir_override);
    const char *image_name = get_adapter_image_name(chip);
    const char *description = get_adapter_description(chip);

    printf("\n--- Adapter Setup for %s ---\n", chip->name);
    printf("%s\n\n", description);

    /* Extract package type */
    const char *at = strchr(chip->name, '@');
    if (at) {
        printf("Package: %s\n", at + 1);
    }

    /* Print voltage info */
    if (chip->voltages_raw) {
        printf("VCC: configured by programmer\n");
    }

    if (!image_dir) {
        printf("\nAdapter images not installed.\n");
        printf("Copy images from Xgpro installer to %s or %s\n",
               LOCAL_IMAGE_DIR, DEFAULT_IMAGE_DIR);
        printf("Expected image: %s\n", image_name);
        return -1;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", image_dir, image_name);

    struct stat st;
    if (stat(path, &st) != 0) {
        /* Try some alternative names */
        const char *alternatives[] = {
            "NoAdapter.jpg",
            "T76_25_WSON8.jpg",
            NULL
        };

        int found = 0;
        for (int i = 0; alternatives[i]; i++) {
            snprintf(path, sizeof(path), "%s/%s", image_dir, alternatives[i]);
            if (stat(path, &st) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            printf("Image '%s' not found in %s\n", image_name, image_dir);
            return -1;
        }
    }

    return open_image(path);
}
