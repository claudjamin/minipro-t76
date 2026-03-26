/*
 * minipro-t76 - Open source programmer for XGecu T76
 *
 * Protocol derived from the open-source minipro project
 * (gitlab.com/DavidGriffith/minipro) and binary analysis of Xgpro_T76.
 *
 * USB: VID 0xA466, PID 0x1A86
 *
 * Endpoint layout (T76-specific):
 *   EP 0x01 OUT / 0x81 IN  - Command/response (msg_send/msg_recv)
 *   EP 0x05 OUT             - Bulk data write (write_payload)
 *   EP 0x82 IN              - Bulk data read  (read_payload)
 */

#ifndef T76_H
#define T76_H

#include <stdint.h>
#include <stddef.h>
#include <libusb-1.0/libusb.h>

/* Global verbose/debug level: 0=quiet, 1=verbose, 2=debug (hex dumps) */
extern int t76_verbose;

/* USB device identifiers */
#define T76_VID              0xA466
#define T76_PID              0x1A86

/* USB endpoints - T76 uses different endpoints than TL866/T48/T56 */
#define T76_USB_INTERFACE    0
#define T76_MSG_OUT_EP       0x01   /* Command messages OUT */
#define T76_MSG_IN_EP        0x81   /* Command messages IN */
#define T76_PAYLOAD_OUT_EP   0x05   /* Bulk data write (T76-specific) */
#define T76_PAYLOAD_IN_EP    0x82   /* Bulk data read (T76-specific) */
#define T76_USB_TIMEOUT      5000   /* ms */
#define T76_USB_READ_TIMEOUT 360000 /* ms, for long operations */

/*
 * T76 Protocol Commands (verified from minipro open-source project)
 *
 * These are identical across the XGecu family (TL866II+/T48/T56/T76).
 * Commands are sent as the first byte of a packet on EP 0x01 OUT.
 * Responses come back on EP 0x81 IN.
 * Bulk data uses EP 0x05 OUT (write) and EP 0x82 IN (read).
 */
#define T76_BEGIN_TRANS       0x03  /* Begin transaction (64-byte config) */
#define T76_END_TRANS         0x04  /* End transaction */
#define T76_READID            0x05  /* Read chip ID */
#define T76_READ_USER         0x06  /* Read user fuses */
#define T76_WRITE_USER        0x07  /* Write user fuses */
#define T76_READ_CFG          0x08  /* Read config fuses */
#define T76_WRITE_CFG         0x09  /* Write config fuses */
#define T76_WRITE_USER_DATA   0x0A  /* Write user data memory */
#define T76_READ_USER_DATA    0x0B  /* Read user data memory */
#define T76_WRITE_CODE        0x0C  /* Write code/flash memory */
#define T76_READ_CODE         0x0D  /* Read code/flash memory */
#define T76_ERASE             0x0E  /* Erase chip */
#define T76_TEST_RAM          0x0F  /* RAM/SRAM test */
#define T76_READ_DATA         0x10  /* Read data/EEPROM memory */
#define T76_WRITE_DATA        0x11  /* Write data/EEPROM memory */
#define T76_WRITE_LOCK        0x14  /* Write lock fuses */
#define T76_READ_LOCK         0x15  /* Read lock fuses */
#define T76_READ_CALIBRATION  0x16  /* Read calibration bytes */
#define T76_PROTECT_OFF       0x18  /* Disable protection */
#define T76_PROTECT_ON        0x19  /* Enable protection */
#define T76_READ_JEDEC        0x1D  /* Read JEDEC row (PLD) */
#define T76_WRITE_JEDEC       0x1E  /* Write JEDEC row (PLD) */
#define T76_WRITE_BITSTREAM   0x26  /* Upload FPGA bitstream */
#define T76_LOGIC_IC_TEST     0x28  /* Logic IC test vector */
#define T76_AUTODETECT        0x37  /* SPI autodetect */
#define T76_UNLOCK_TSOP48     0x38  /* Unlock TSOP48 adapter */
#define T76_REQUEST_STATUS    0x39  /* Get OVC/error status */
#define T76_BOOTLOADER_WRITE  0x3B  /* Write firmware block */
#define T76_BOOTLOADER_ERASE  0x3C  /* Erase firmware */
#define T76_SWITCH            0x3D  /* Switch to bootloader mode */
#define T76_PIN_DETECTION     0x3E  /* Pin detection */

/* Bitstream sub-commands for T76_WRITE_BITSTREAM */
#define T76_BEGIN_BS          0x00  /* Begin bitstream upload */
#define T76_BS_BLOCK          0x01  /* Bitstream data block */
#define T76_END_BS            0x02  /* End bitstream upload */
#define T76_RESET_FPGA        0xAF  /* Reset FPGA */
#define T76_FPGA_MAGIC        0xAA55DDEE

/* Bitstream packet size */
#define BS_PACKET_SIZE        0x200

/* Memory types */
#define MP_CODE               0x00
#define MP_DATA               0x01
#define MP_USER               0x02

/* Fuse types */
#define MP_FUSE_USER          0x00
#define MP_FUSE_CFG           0x01
#define MP_FUSE_LOCK          0x02

/* Chip categories */
#define MP_MEMORY             0x01
#define MP_MCU                0x02
#define MP_PLD                0x03
#define MP_SRAM               0x04
#define MP_LOGIC              0x05
#define MP_NAND               0x06
#define MP_EMMC               0x07
#define MP_VGA                0x08

/* Byte order */
#define MP_LITTLE_ENDIAN      0
#define MP_BIG_ENDIAN         1

/* ICSP */
#define MP_ICSP_ENABLE        0x80
#define MP_ICSP_VCC           0x01

/* Connection mode */
#define MP_ZIF_ONLY           0x00
#define MP_ZIF_ICSP           0x01
#define MP_ICSP_ONLY          0x02

/* ID types */
#define MP_ID_TYPE1           0x01
#define MP_ID_TYPE2           0x02
#define MP_ID_TYPE3           0x03
#define MP_ID_TYPE4           0x04
#define MP_ID_TYPE5           0x05

/* SPI autodetect algorithm numbers */
#define SPI_DEVICE_8P         0x11
#define SPI_DEVICE_16P        0x21
#define SPI_PROTOCOL          0x03

/* Firmware */
#define T76_FIRMWARE_VERSION  0x10D
#define T76_FIRMWARE_STRING   "00.1.13"
#define T76_UPDATE_FILE_VERSION 0xF0760000
#define T76_BTLDR_MAGIC       0x049000

/* Chip database entry */
typedef struct {
    char name[40];
    uint32_t chip_type;
    uint8_t  protocol_id;
    uint32_t variant;
    uint16_t read_buffer_size;
    uint16_t write_buffer_size;
    uint32_t code_memory_size;
    uint32_t data_memory_size;
    uint32_t data_memory2_size;
    uint32_t page_size;
    uint32_t pages_per_block;  /* NAND only */
    uint32_t chip_id;
    uint8_t  chip_id_bytes_count;
    uint32_t voltages_raw;
    uint32_t pulse_delay;
    uint32_t flags_raw;
    uint32_t chip_info;
    uint32_t pin_map;
    uint16_t compare_mask;
    uint16_t blank_value;
    uint32_t package_details;  /* pin count, adapter, icsp */
    uint8_t  spi_clock;
    uint8_t  i2c_address;
    char     adapter_image[64]; /* adapter/setup image filename */
    char     algo_name[16];     /* FPGA algorithm name */
} chip_t;

/* OVC status */
typedef struct {
    uint8_t  error;
    uint32_t address;
    uint32_t c1;
    uint32_t c2;
} t76_status_t;

/* Device handle */
typedef struct {
    libusb_device_handle *usb_handle;
    libusb_context *usb_ctx;
    char model[32];
    char firmware_str[16];
    char device_code[9];
    char serial_number[25];
    uint32_t firmware;
    uint8_t  hw_version;
    uint8_t  status;     /* 1=normal, 2=bootloader */
    uint8_t  version;    /* protocol version (8 for T76) */
    int is_connected;
    int bitstream_uploaded;
} t76_handle_t;

/* Device management */
int t76_open(t76_handle_t *dev);
void t76_close(t76_handle_t *dev);
int t76_get_device_info(t76_handle_t *dev);
void t76_print_device_info(t76_handle_t *dev);

/* Low-level USB transport */
int t76_msg_send(t76_handle_t *dev, uint8_t *data, size_t len);
int t76_msg_recv(t76_handle_t *dev, uint8_t *data, size_t len);
int t76_write_payload(t76_handle_t *dev, uint8_t *data, size_t len);
int t76_read_payload(t76_handle_t *dev, uint8_t *data, size_t len);

/* Byte packing helpers */
void format_int(uint8_t *out, uint64_t in, size_t size, uint8_t endianness);
uint64_t load_int(uint8_t *buffer, size_t size, uint8_t endianness);

/* Chip operations */
int t76_begin_transaction(t76_handle_t *dev, chip_t *chip);
int t76_end_transaction(t76_handle_t *dev);
int t76_get_chip_id(t76_handle_t *dev, chip_t *chip, uint8_t *type, uint32_t *device_id);
int t76_spi_autodetect(t76_handle_t *dev, uint8_t type, uint32_t *device_id);
int t76_read_block(t76_handle_t *dev, chip_t *chip, uint8_t mem_type,
                   uint32_t addr, uint8_t *buf, size_t len, int is_first);
int t76_write_block(t76_handle_t *dev, chip_t *chip, uint8_t mem_type,
                    uint32_t addr, uint8_t *buf, size_t len, int is_first);
int t76_erase(t76_handle_t *dev, chip_t *chip);
int t76_get_ovc_status(t76_handle_t *dev, t76_status_t *status, uint8_t *ovc);
int t76_request_status(t76_handle_t *dev, t76_status_t *status);

/* Protection */
int t76_protect_off(t76_handle_t *dev);
int t76_protect_on(t76_handle_t *dev);

/* Fuses */
int t76_read_fuses(t76_handle_t *dev, uint8_t type, size_t size,
                   uint8_t items_count, uint8_t *buffer);
int t76_write_fuses(t76_handle_t *dev, uint8_t type, size_t size,
                    uint8_t items_count, uint8_t *buffer);
int t76_read_calibration(t76_handle_t *dev, uint8_t *buffer, size_t len);

/* FPGA bitstream */
int t76_write_bitstream(t76_handle_t *dev, uint8_t *bitstream, size_t length);
int t76_reset_fpga(t76_handle_t *dev);
int t76_load_algorithm(t76_handle_t *dev, chip_t *chip, const char *algo_dir);

/* Chip database */
int chipdb_load(const char *path);
void chipdb_free(void);
chip_t *chipdb_find(const char *name);
void chipdb_list(const char *filter);
int chipdb_count(void);

/* Adapter/setup image display */
int show_adapter_image(chip_t *chip, const char *image_dir);
const char *get_adapter_image_name(chip_t *chip);

/* File I/O */
typedef enum {
    FMT_AUTO = 0,
    FMT_BIN,
    FMT_IHEX,
    FMT_SREC,
} file_format_t;

int file_read_buf(const char *path, file_format_t fmt, uint8_t *buf,
                  uint32_t buf_size, uint32_t *data_len);
int file_write_buf(const char *path, file_format_t fmt, uint8_t *buf,
                   uint32_t data_len);
file_format_t file_detect_format(const char *path);

#endif /* T76_H */
