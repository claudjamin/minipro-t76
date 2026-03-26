/*
 * minipro-t76 - T76 programming protocol implementation
 *
 * Based on the verified protocol from the minipro open-source project.
 * Commands go on EP 0x01, bulk data on EP 0x05 (write) / 0x82 (read).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "t76.h"

/* Maximum single block transfer size (256KB should cover any chip) */
#define MAX_BLOCK_SIZE (256 * 1024)

/* Progress bar */
static void print_progress(size_t current, size_t total, const char *label)
{
    int pct = (int)((uint64_t)current * 100 / total);
    int bar_len = 40;
    int filled = pct * bar_len / 100;

    printf("\r%s: [", label);
    for (int i = 0; i < bar_len; i++)
        putchar(i < filled ? '#' : '-');
    printf("] %d%%", pct);

    if (current >= total)
        printf("\n");
    fflush(stdout);
}

/*
 * t76_begin_transaction - Configure the programmer for a specific chip
 *
 * Sends a 64-byte packet on EP 0x01 with chip parameters:
 *   [0]     = 0x03 (T76_BEGIN_TRANS)
 *   [1]     = protocol_id
 *   [2]     = variant (low byte)
 *   [3]     = ICSP mode
 *   [4..5]  = raw_voltages (LE16)
 *   [6]     = chip_info
 *   [7]     = pin_map
 *   [8..9]  = data_memory_size (LE16)
 *   [10..11]= page_size (LE16)
 *   [12..13]= pulse_delay (LE16)
 *   [14..15]= data_memory2_size (LE16)
 *   [16..19]= code_memory_size (LE32)
 *   [20]    = voltages byte2
 *   [21..22]= voltage split
 *   [24]    = i2c_address (if adjustable)
 *   [28]    = spi_clock (if adjustable)
 *   [40..43]= package_details (LE32)
 *   [44..45]= read_buffer_size (LE16)
 *   [56..59]= raw_flags (LE32)
 *   [63]    = algorithm number (variant >> 8)
 */
int t76_begin_transaction(t76_handle_t *dev, chip_t *chip)
{
    uint8_t msg[64] = { 0 };
    uint8_t ovc;

    msg[0] = T76_BEGIN_TRANS;
    msg[1] = chip->protocol_id;
    msg[2] = (uint8_t)chip->variant;
    msg[3] = 0; /* ICSP mode - 0 for ZIF */

    format_int(&msg[4], chip->voltages_raw, 2, MP_LITTLE_ENDIAN);
    msg[6] = (uint8_t)chip->chip_info;
    msg[7] = (uint8_t)chip->pin_map;
    format_int(&msg[8], chip->data_memory_size, 2, MP_LITTLE_ENDIAN);
    format_int(&msg[10], chip->page_size, 2, MP_LITTLE_ENDIAN);
    format_int(&msg[12], chip->pulse_delay, 2, MP_LITTLE_ENDIAN);
    format_int(&msg[14], chip->data_memory2_size, 2, MP_LITTLE_ENDIAN);
    format_int(&msg[16], chip->code_memory_size, 4, MP_LITTLE_ENDIAN);

    msg[20] = (uint8_t)(chip->voltages_raw >> 16);

    if ((chip->voltages_raw & 0xf0) == 0xf0) {
        msg[22] = (uint8_t)chip->voltages_raw;
    } else {
        msg[21] = (uint8_t)chip->voltages_raw & 0x0f;
        msg[22] = (uint8_t)chip->voltages_raw & 0xf0;
    }
    if (chip->voltages_raw & 0x80000000)
        msg[22] = (chip->voltages_raw >> 16) & 0x0f;

    msg[24] = chip->i2c_address;
    msg[28] = chip->spi_clock;

    format_int(&msg[40], chip->package_details, 4, MP_LITTLE_ENDIAN);
    format_int(&msg[44], chip->read_buffer_size, 2, MP_LITTLE_ENDIAN);
    format_int(&msg[56], chip->flags_raw, 4, MP_LITTLE_ENDIAN);

    /* Algorithm number is high byte of variant */
    msg[63] = (uint8_t)(chip->variant >> 8);

    if (t76_msg_send(dev, msg, sizeof(msg)))
        return -1;

    /* Check overcurrent */
    if (t76_get_ovc_status(dev, NULL, &ovc))
        return -1;
    if (ovc) {
        fprintf(stderr, "Overcurrent protection!\n");
        return -1;
    }

    return 0;
}

int t76_end_transaction(t76_handle_t *dev)
{
    uint8_t msg[8] = { 0 };
    msg[0] = T76_END_TRANS;
    return t76_msg_send(dev, msg, sizeof(msg));
}

int t76_get_ovc_status(t76_handle_t *dev, t76_status_t *status, uint8_t *ovc)
{
    uint8_t msg[32] = { 0 };

    msg[0] = T76_REQUEST_STATUS;
    if (t76_msg_send(dev, msg, 8))
        return -1;

    memset(msg, 0, sizeof(msg));
    if (t76_msg_recv(dev, msg, sizeof(msg)))
        return -1;

    if (status) {
        status->error = msg[0];
        status->address = load_int(&msg[4], 4, MP_LITTLE_ENDIAN);
        status->c1 = load_int(&msg[8], 4, MP_LITTLE_ENDIAN);
        status->c2 = load_int(&msg[12], 4, MP_LITTLE_ENDIAN);
    }
    if (ovc)
        *ovc = msg[0];

    return 0;
}

int t76_get_chip_id(t76_handle_t *dev, chip_t *chip, uint8_t *type,
                    uint32_t *device_id)
{
    uint8_t msg[16] = { 0 };

    msg[0] = T76_READID;
    msg[1] = chip->chip_id_bytes_count;

    if (t76_msg_send(dev, msg, 8))
        return -1;
    if (t76_msg_recv(dev, msg, sizeof(msg)))
        return -1;

    if (type)
        *type = msg[0];
    if (device_id) {
        /* Chip ID is in bytes 1..4 depending on chip_id_bytes_count */
        *device_id = 0;
        for (int i = 0; i < chip->chip_id_bytes_count && i < 4; i++)
            *device_id |= (uint32_t)msg[1 + i] << (i * 8);
    }

    return 0;
}

int t76_spi_autodetect(t76_handle_t *dev, uint8_t type, uint32_t *device_id)
{
    uint8_t msg[16] = { 0 };

    msg[0] = T76_AUTODETECT;
    msg[1] = type;

    if (t76_msg_send(dev, msg, 8))
        return -1;
    if (t76_msg_recv(dev, msg, sizeof(msg)))
        return -1;

    if (device_id)
        *device_id = load_int(&msg[2], 4, MP_LITTLE_ENDIAN);

    return (msg[0] != 0) ? -1 : 0;
}

/*
 * Read a block of memory from the device.
 *
 * For MP_CODE: command on EP 0x01, data comes back on EP 0x82
 * For MP_DATA: command on EP 0x01, data comes back on EP 0x82 with 16-byte prefix
 */
int t76_read_block(t76_handle_t *dev, chip_t *chip, uint8_t mem_type,
                   uint32_t addr, uint8_t *buf, size_t len, int is_first)
{
    uint8_t msg[64] = { 0 };

    (void)chip;

    if (len > MAX_BLOCK_SIZE) {
        fprintf(stderr, "Error: read block size %zu exceeds maximum\n", len);
        return -1;
    }

    if (mem_type == MP_CODE) {
        msg[0] = T76_READ_CODE;
        format_int(&msg[2], len, 2, MP_LITTLE_ENDIAN);
        format_int(&msg[4], addr, 4, MP_LITTLE_ENDIAN);

        /* For first block, also send block count and issue the command */
        if (is_first) {
            /* block_count = total blocks to read */
            if (t76_msg_send(dev, msg, 16))
                return -1;
        }

        return t76_read_payload(dev, buf, len);

    } else if (mem_type == MP_DATA) {
        msg[0] = T76_READ_DATA;
        format_int(&msg[2], len, 2, MP_LITTLE_ENDIAN);
        format_int(&msg[4], addr, 4, MP_LITTLE_ENDIAN);

        if (t76_msg_send(dev, msg, 16))
            return -1;

        /* Data comes with 16-byte prefix */
        uint8_t *data = malloc(len + 16);
        if (!data) {
            fprintf(stderr, "Out of memory!\n");
            return -1;
        }

        int ret = t76_read_payload(dev, data, len + 16);
        if (ret == 0)
            memcpy(buf, data + 16, len);
        free(data);
        return ret;

    } else if (mem_type == MP_USER) {
        msg[0] = T76_READ_USER_DATA;
        format_int(&msg[2], len, 2, MP_LITTLE_ENDIAN);
        format_int(&msg[4], addr, 4, MP_LITTLE_ENDIAN);

        if (t76_msg_send(dev, msg, 16))
            return -1;

        uint8_t *data = malloc(len + 16);
        if (!data) {
            fprintf(stderr, "Out of memory!\n");
            return -1;
        }

        int ret = t76_msg_recv(dev, data, len + 16);
        if (ret == 0)
            memcpy(buf, data + 16, len);
        free(data);
        return ret;
    }

    fprintf(stderr, "Unknown type for read_block (%d)\n", mem_type);
    return -1;
}

/*
 * Write a block of memory to the device.
 *
 * For MP_CODE: command on EP 0x01, data goes on EP 0x05
 * The data packet includes a 16-byte header prepended to the actual data.
 */
int t76_write_block(t76_handle_t *dev, chip_t *chip, uint8_t mem_type,
                    uint32_t addr, uint8_t *buf, size_t len, int is_first)
{
    uint8_t msg[64] = { 0 };

    (void)chip;

    if (len > MAX_BLOCK_SIZE) {
        fprintf(stderr, "Error: write block size %zu exceeds maximum\n", len);
        return -1;
    }

    format_int(&msg[2], len, 2, MP_LITTLE_ENDIAN);
    format_int(&msg[4], addr, 4, MP_LITTLE_ENDIAN);
    format_int(&msg[12], len, 4, MP_LITTLE_ENDIAN);

    if (mem_type == MP_CODE) {
        msg[0] = T76_WRITE_CODE;

        if (is_first) {
            if (t76_msg_send(dev, msg, 16))
                return -1;
        }

        /* Prepend 16-byte header to data and send via EP 0x05 */
        memset(&msg[8], 0, 4);
        uint8_t *data = malloc(len + 16);
        if (!data) {
            fprintf(stderr, "Out of memory!\n");
            return -1;
        }

        memcpy(data, msg, 16);
        memcpy(data + 16, buf, len);
        int ret = t76_write_payload(dev, data, len + 16);
        free(data);
        return ret;

    } else if (mem_type == MP_DATA) {
        msg[0] = T76_WRITE_DATA;
        format_int(&msg[2], len, 2, MP_LITTLE_ENDIAN);
        format_int(&msg[4], addr, 4, MP_LITTLE_ENDIAN);

        if (t76_msg_send(dev, msg, 16))
            return -1;

        uint8_t *data = malloc(len + 16);
        if (!data) {
            fprintf(stderr, "Out of memory!\n");
            return -1;
        }

        memcpy(data, msg, 16);
        memcpy(data + 16, buf, len);
        int ret = t76_write_payload(dev, data, len + 16);
        free(data);
        return ret;
    }

    fprintf(stderr, "Unknown type for write_block (%d)\n", mem_type);
    return -1;
}

int t76_erase(t76_handle_t *dev, chip_t *chip)
{
    uint8_t msg[64] = { 0 };
    t76_status_t status;
    uint8_t ovc;

    (void)chip;

    printf("Erasing chip...\n");

    msg[0] = T76_ERASE;
    if (t76_msg_send(dev, msg, 8))
        return -1;

    /* Wait for erase completion by polling status */
    if (t76_msg_recv(dev, msg, sizeof(msg)))
        return -1;

    /* Check for errors */
    if (t76_get_ovc_status(dev, &status, &ovc))
        return -1;

    if (ovc) {
        fprintf(stderr, "Overcurrent during erase!\n");
        return -1;
    }

    if (status.error) {
        fprintf(stderr, "Erase error (status=0x%02X)\n", status.error);
        return -1;
    }

    printf("Erase complete.\n");
    return 0;
}

int t76_protect_off(t76_handle_t *dev)
{
    uint8_t msg[8] = { 0 };
    msg[0] = T76_PROTECT_OFF;
    if (t76_msg_send(dev, msg, sizeof(msg)))
        return -1;
    return t76_msg_recv(dev, msg, sizeof(msg));
}

int t76_protect_on(t76_handle_t *dev)
{
    uint8_t msg[8] = { 0 };
    msg[0] = T76_PROTECT_ON;
    if (t76_msg_send(dev, msg, sizeof(msg)))
        return -1;
    return t76_msg_recv(dev, msg, sizeof(msg));
}

int t76_read_fuses(t76_handle_t *dev, uint8_t type, size_t size,
                   uint8_t items_count, uint8_t *buffer)
{
    uint8_t msg[64] = { 0 };
    uint8_t cmd;

    if (size > 63) {
        fprintf(stderr, "Error: fuse read size %zu exceeds maximum (63)\n", size);
        return -1;
    }

    switch (type) {
    case MP_FUSE_USER: cmd = T76_READ_USER; break;
    case MP_FUSE_CFG:  cmd = T76_READ_CFG; break;
    case MP_FUSE_LOCK: cmd = T76_READ_LOCK; break;
    default:
        fprintf(stderr, "Unknown fuse type %d\n", type);
        return -1;
    }

    msg[0] = cmd;
    msg[1] = items_count;
    format_int(&msg[2], size, 2, MP_LITTLE_ENDIAN);

    if (t76_msg_send(dev, msg, 8))
        return -1;
    if (t76_msg_recv(dev, msg, sizeof(msg)))
        return -1;

    memcpy(buffer, &msg[1], size);
    return 0;
}

int t76_write_fuses(t76_handle_t *dev, uint8_t type, size_t size,
                    uint8_t items_count, uint8_t *buffer)
{
    uint8_t msg[64] = { 0 };
    uint8_t cmd;

    switch (type) {
    case MP_FUSE_USER: cmd = T76_WRITE_USER; break;
    case MP_FUSE_CFG:  cmd = T76_WRITE_CFG; break;
    case MP_FUSE_LOCK: cmd = T76_WRITE_LOCK; break;
    default:
        fprintf(stderr, "Unknown fuse type %d\n", type);
        return -1;
    }

    if (size > 60) {
        fprintf(stderr, "Error: fuse write size %zu exceeds maximum (60)\n", size);
        return -1;
    }

    msg[0] = cmd;
    msg[1] = items_count;
    format_int(&msg[2], size, 2, MP_LITTLE_ENDIAN);
    memcpy(&msg[4], buffer, size);

    if (t76_msg_send(dev, msg, sizeof(msg)))
        return -1;
    return t76_msg_recv(dev, msg, sizeof(msg));
}

int t76_read_calibration(t76_handle_t *dev, uint8_t *buffer, size_t len)
{
    uint8_t msg[64] = { 0 };

    if (len > 63) {
        fprintf(stderr, "Error: calibration read size %zu exceeds maximum (63)\n", len);
        return -1;
    }

    msg[0] = T76_READ_CALIBRATION;
    format_int(&msg[2], len, 2, MP_LITTLE_ENDIAN);

    if (t76_msg_send(dev, msg, 8))
        return -1;
    if (t76_msg_recv(dev, msg, sizeof(msg)))
        return -1;

    memcpy(buffer, &msg[1], len);
    return 0;
}

/*
 * Upload FPGA bitstream - T76-specific
 *
 * The T76 has an Anlogic FPGA that needs a bitstream loaded for each
 * chip type. The bitstream defines the pin mapping and I/O configuration.
 */
int t76_write_bitstream(t76_handle_t *dev, uint8_t *bitstream, size_t length)
{
    uint8_t msg[BS_PACKET_SIZE];
    size_t payload_size = BS_PACKET_SIZE - 8;

    /* Phase 1: Begin bitstream */
    memset(msg, 0, sizeof(msg));
    msg[0] = T76_WRITE_BITSTREAM;
    msg[1] = T76_BEGIN_BS;
    format_int(&msg[2], BS_PACKET_SIZE, 2, MP_LITTLE_ENDIAN);
    format_int(&msg[4], length, 4, MP_LITTLE_ENDIAN);

    if (t76_msg_send(dev, msg, 8))
        return -1;

    /* Check response */
    if (t76_msg_recv(dev, msg, 8) || msg[1])
        return -1;

    /* Phase 2: Send bitstream in 512-byte chunks (8 header + 504 data) */
    for (size_t i = 0; i < length; i += payload_size) {
        size_t block_size = ((i + payload_size) <= length)
            ? payload_size : (length - i);

        memset(msg, 0, sizeof(msg));
        msg[0] = T76_WRITE_BITSTREAM;
        msg[1] = T76_BS_BLOCK;
        format_int(&msg[2], block_size, 2, MP_LITTLE_ENDIAN);
        memcpy(&msg[8], &bitstream[i], block_size);

        if (t76_msg_send(dev, msg, BS_PACKET_SIZE))
            return -1;
    }

    /* Phase 3: End bitstream */
    memset(msg, 0, sizeof(msg));
    msg[0] = T76_WRITE_BITSTREAM;
    msg[1] = T76_END_BS;

    if (t76_msg_send(dev, msg, 8))
        return -1;

    /* Check final status */
    if (t76_msg_recv(dev, msg, 8) || msg[1])
        return -1;

    return 0;
}

int t76_reset_fpga(t76_handle_t *dev)
{
    uint8_t msg[8] = { 0 };

    msg[0] = T76_WRITE_BITSTREAM;
    msg[1] = T76_RESET_FPGA;
    format_int(&msg[4], T76_FPGA_MAGIC, 4, MP_LITTLE_ENDIAN);

    if (t76_msg_send(dev, msg, 8))
        return -1;

    return (t76_msg_recv(dev, msg, 8) || msg[1]) ? -1 : 0;
}

/*
 * High-level read: reads entire code memory with progress bar
 */
int t76_read_code_memory(t76_handle_t *dev, chip_t *chip, uint8_t *buf)
{
    uint32_t total = chip->code_memory_size;
    uint32_t block_size = chip->read_buffer_size;
    uint32_t offset = 0;

    if (!block_size)
        block_size = 256;

    while (offset < total) {
        uint32_t chunk = total - offset;
        if (chunk > block_size)
            chunk = block_size;

        if (t76_read_block(dev, chip, MP_CODE, offset, buf + offset,
                           chunk, (offset == 0)))
            return -1;

        offset += chunk;
        print_progress(offset, total, "Reading");
    }

    return 0;
}

/*
 * High-level write: writes code memory with progress bar
 */
int t76_write_code_memory(t76_handle_t *dev, chip_t *chip, uint8_t *buf,
                          uint32_t len)
{
    uint32_t block_size = chip->write_buffer_size;
    uint32_t offset = 0;

    if (!block_size)
        block_size = 256;

    while (offset < len) {
        uint32_t chunk = len - offset;
        if (chunk > block_size)
            chunk = block_size;

        if (t76_write_block(dev, chip, MP_CODE, offset, buf + offset,
                            chunk, (offset == 0)))
            return -1;

        offset += chunk;
        print_progress(offset, len, "Writing");
    }

    /* Check status after write */
    t76_status_t status;
    uint8_t ovc;
    if (t76_get_ovc_status(dev, &status, &ovc))
        return -1;
    if (ovc || status.error) {
        fprintf(stderr, "Write error at address 0x%08X\n", status.address);
        return -1;
    }

    return 0;
}
