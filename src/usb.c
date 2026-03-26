/*
 * minipro-t76 - USB communication layer
 *
 * T76 endpoint layout:
 *   EP 0x01 OUT / 0x81 IN  - Commands (msg_send / msg_recv)
 *   EP 0x05 OUT             - Write payload (bulk data to device)
 *   EP 0x82 IN              - Read payload (bulk data from device)
 *
 * This differs from TL866/T48/T56 which use EP 0x02/0x03 for payload.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "t76.h"

/* Pack integer to buffer in specified endianness */
void format_int(uint8_t *out, uint64_t in, size_t size, uint8_t endianness)
{
    for (size_t i = 0; i < size; i++) {
        if (endianness == MP_LITTLE_ENDIAN)
            out[i] = (in >> (i * 8)) & 0xFF;
        else
            out[size - 1 - i] = (in >> (i * 8)) & 0xFF;
    }
}

/* Load integer from buffer in specified endianness */
uint64_t load_int(uint8_t *buffer, size_t size, uint8_t endianness)
{
    uint64_t result = 0;
    for (size_t i = 0; i < size; i++) {
        if (endianness == MP_LITTLE_ENDIAN)
            result |= (uint64_t)buffer[i] << (i * 8);
        else
            result |= (uint64_t)buffer[size - 1 - i] << (i * 8);
    }
    return result;
}

int t76_open(t76_handle_t *dev)
{
    int ret;

    memset(dev, 0, sizeof(*dev));

    ret = libusb_init(&dev->usb_ctx);
    if (ret < 0) {
        fprintf(stderr, "Error: libusb_init failed: %s\n",
                libusb_strerror(ret));
        return -1;
    }

    dev->usb_handle = libusb_open_device_with_vid_pid(
        dev->usb_ctx, T76_VID, T76_PID);
    if (!dev->usb_handle) {
        fprintf(stderr, "Error: XGecu T76 not found (VID=%04X PID=%04X)\n",
                T76_VID, T76_PID);
        fprintf(stderr, "Make sure the programmer is connected and you have "
                "permission to access USB devices.\n");
        fprintf(stderr, "Try: sudo minipro-t76 ... or install the udev rule.\n");
        libusb_exit(dev->usb_ctx);
        return -1;
    }

    /* Detach kernel driver if attached */
    if (libusb_kernel_driver_active(dev->usb_handle, T76_USB_INTERFACE) == 1) {
        ret = libusb_detach_kernel_driver(dev->usb_handle, T76_USB_INTERFACE);
        if (ret < 0) {
            fprintf(stderr, "Warning: could not detach kernel driver: %s\n",
                    libusb_strerror(ret));
        }
    }

    ret = libusb_claim_interface(dev->usb_handle, T76_USB_INTERFACE);
    if (ret < 0) {
        fprintf(stderr, "Error: cannot claim USB interface: %s\n",
                libusb_strerror(ret));
        libusb_close(dev->usb_handle);
        libusb_exit(dev->usb_ctx);
        return -1;
    }

    dev->is_connected = 1;

    /* Read device info */
    ret = t76_get_device_info(dev);
    if (ret < 0)
        fprintf(stderr, "Warning: could not read device info\n");

    return 0;
}

void t76_close(t76_handle_t *dev)
{
    if (!dev || !dev->is_connected)
        return;

    libusb_release_interface(dev->usb_handle, T76_USB_INTERFACE);
    libusb_close(dev->usb_handle);
    libusb_exit(dev->usb_ctx);
    dev->is_connected = 0;
}

/* Hex dump helper for debug output */
static void hex_dump(const char *label, uint8_t *data, int len, uint8_t ep)
{
    if (t76_verbose < 2)
        return;
    fprintf(stderr, "\033[33m%s %d bytes on EP 0x%02X\033[0m\n", label, len, ep);
    for (int i = 0; i < len; i += 16) {
        fprintf(stderr, "  %04X: ", i);
        for (int j = 0; j < 16; j++) {
            if (i + j < len)
                fprintf(stderr, "\033[36m%02X \033[0m", data[i + j]);
            else
                fprintf(stderr, "   ");
        }
        fprintf(stderr, " ");
        for (int j = 0; j < 16 && i + j < len; j++) {
            uint8_t c = data[i + j];
            fprintf(stderr, "%c", (c >= 32 && c < 127) ? c : '.');
        }
        fprintf(stderr, "\n");
    }
}

/*
 * msg_send - Send command message on EP 0x01 OUT
 */
int t76_msg_send(t76_handle_t *dev, uint8_t *data, size_t len)
{
    int transferred = 0;

    if (!dev || !dev->is_connected)
        return -1;

    hex_dump("Write", data, len > 64 ? 64 : len, T76_MSG_OUT_EP);

    int ret = libusb_bulk_transfer(dev->usb_handle, T76_MSG_OUT_EP,
                                   data, len, &transferred, T76_USB_TIMEOUT);
    if (ret < 0) {
        fprintf(stderr, "USB msg_send error: %s\n", libusb_strerror(ret));
        return -1;
    }

    if (transferred != (int)len) {
        fprintf(stderr, "USB msg_send: short write %d/%zu\n", transferred, len);
        return -1;
    }

    return 0;
}

/*
 * msg_recv - Receive response on EP 0x81 IN
 */
int t76_msg_recv(t76_handle_t *dev, uint8_t *data, size_t len)
{
    int transferred = 0;

    if (!dev || !dev->is_connected)
        return -1;

    int ret = libusb_bulk_transfer(dev->usb_handle, T76_MSG_IN_EP,
                                   data, len, &transferred,
                                   T76_USB_READ_TIMEOUT);
    if (ret < 0) {
        fprintf(stderr, "USB msg_recv error: %s\n", libusb_strerror(ret));
        return -1;
    }

    if (t76_verbose >= 2)
        fprintf(stderr, "\033[33mRead %d bytes (requested %zu) on EP 0x%02X\033[0m\n",
                transferred, len, T76_MSG_IN_EP);
    hex_dump("Read", data, transferred > 64 ? 64 : transferred, T76_MSG_IN_EP);

    return 0;
}

/*
 * write_payload - Send bulk data on EP 0x05 OUT (T76-specific)
 */
int t76_write_payload(t76_handle_t *dev, uint8_t *data, size_t len)
{
    int transferred = 0;

    if (!dev || !dev->is_connected)
        return -1;

    if (t76_verbose >= 2)
        hex_dump("PayloadWrite", data, len > 64 ? 64 : len, T76_PAYLOAD_OUT_EP);

    int ret = libusb_bulk_transfer(dev->usb_handle, T76_PAYLOAD_OUT_EP,
                                   data, len, &transferred, T76_USB_TIMEOUT);
    if (ret < 0) {
        fprintf(stderr, "USB write_payload error: %s\n", libusb_strerror(ret));
        return -1;
    }

    return 0;
}

/*
 * read_payload - Receive bulk data on EP 0x82 IN (T76-specific)
 */
int t76_read_payload(t76_handle_t *dev, uint8_t *data, size_t len)
{
    int transferred = 0;

    if (!dev || !dev->is_connected)
        return -1;

    int ret = libusb_bulk_transfer(dev->usb_handle, T76_PAYLOAD_IN_EP,
                                   data, len, &transferred, T76_USB_TIMEOUT);
    if (ret < 0) {
        fprintf(stderr, "USB read_payload error: %s\n", libusb_strerror(ret));
        return -1;
    }

    if (t76_verbose >= 2)
        hex_dump("PayloadRead", data, transferred > 64 ? 64 : transferred, T76_PAYLOAD_IN_EP);

    return 0;
}

/*
 * Get device information - the response tells us firmware version,
 * hardware version, serial number, device code, etc.
 *
 * Protocol version 8 = T76
 */
int t76_get_device_info(t76_handle_t *dev)
{
    uint8_t msg[8] = { 0 };
    uint8_t response[64] = { 0 };

    /* Send empty 8-byte packet on EP 0x01 to request info
     * (command byte 0x00 = get device info) */
    if (t76_msg_send(dev, msg, sizeof(msg)))
        return -1;

    if (t76_msg_recv(dev, response, sizeof(response)))
        return -1;

    /*
     * Response layout (from minipro source):
     * [0]      status (1=normal, 2=bootloader)
     * [1..4]   firmware version
     * [5]      hardware version
     * [6]      protocol version (8 for T76)
     * [7]      speed
     * [8..15]  device code
     * [16..39] serial number
     * [40..47] reserved
     * [48..63] reserved
     */
    dev->status = response[0];
    dev->firmware = load_int(&response[1], 4, MP_LITTLE_ENDIAN);
    dev->hw_version = response[5];
    dev->version = response[6];

    snprintf(dev->firmware_str, sizeof(dev->firmware_str), "%02d.%d.%02d",
             (dev->firmware >> 8) & 0xFF,
             (dev->firmware >> 4) & 0x0F,
             dev->firmware & 0x0F);

    memcpy(dev->device_code, &response[8], 8);
    dev->device_code[8] = '\0';

    memcpy(dev->serial_number, &response[16], 24);
    dev->serial_number[24] = '\0';

    if (dev->version == 8)
        strncpy(dev->model, "XGecu T76", sizeof(dev->model));
    else
        snprintf(dev->model, sizeof(dev->model), "Unknown (ver=%d)", dev->version);

    return 0;
}

void t76_print_device_info(t76_handle_t *dev)
{
    if (!dev || !dev->is_connected) {
        printf("No programmer connected.\n");
        return;
    }

    printf("Found %s\n", dev->model);
    printf("  Firmware: %s (0x%X)\n", dev->firmware_str, dev->firmware);
    printf("  Hardware: %d\n", dev->hw_version);
    printf("  Protocol: %d\n", dev->version);
    if (dev->device_code[0])
        printf("  Device code: %s\n", dev->device_code);
    if (dev->serial_number[0])
        printf("  Serial: %s\n", dev->serial_number);
    printf("  Status: %s\n",
           dev->status == 1 ? "Normal" :
           dev->status == 2 ? "Bootloader" : "Unknown");
}
