/*
 * minipro-t76 - File I/O for binary, Intel HEX, and SREC formats
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "t76.h"

/* Binary file I/O */
int file_read_bin(const char *path, uint8_t *buf, uint32_t buf_size,
                  uint32_t *data_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s' for reading\n", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize < 0) {
        fclose(f);
        return -1;
    }

    if ((uint32_t)fsize > buf_size) {
        fprintf(stderr, "Error: file size (%ld) exceeds buffer (%u)\n",
                fsize, buf_size);
        fclose(f);
        return -1;
    }

    size_t nread = fread(buf, 1, fsize, f);
    fclose(f);

    if (nread != (size_t)fsize) {
        fprintf(stderr, "Error: short read (%zu of %ld bytes)\n", nread, fsize);
        return -1;
    }

    *data_len = (uint32_t)nread;
    return 0;
}

int file_write_bin(const char *path, uint8_t *buf, uint32_t data_len)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s' for writing\n", path);
        return -1;
    }

    size_t written = fwrite(buf, 1, data_len, f);
    fclose(f);

    if (written != data_len) {
        fprintf(stderr, "Error: short write (%zu of %u bytes)\n",
                written, data_len);
        return -1;
    }

    return 0;
}

/* Intel HEX parser */
static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int hex_byte(const char *s)
{
    int hi = hex_nibble(s[0]);
    int lo = hex_nibble(s[1]);
    if (hi < 0 || lo < 0) return -1;
    return (hi << 4) | lo;
}

int file_read_intel_hex(const char *path, uint8_t *buf, uint32_t buf_size,
                        uint32_t *data_len)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s' for reading\n", path);
        return -1;
    }

    char line[600];
    uint32_t base_addr = 0;
    uint32_t max_addr = 0;
    int line_num = 0;

    memset(buf, 0xFF, buf_size);

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        char *p = line;

        /* Skip whitespace */
        while (*p && isspace(*p)) p++;
        if (*p != ':') continue;
        p++;

        int byte_count = hex_byte(p); p += 2;
        int addr_hi = hex_byte(p); p += 2;
        int addr_lo = hex_byte(p); p += 2;
        int rec_type = hex_byte(p); p += 2;

        if (byte_count < 0 || addr_hi < 0 || addr_lo < 0 || rec_type < 0) {
            fprintf(stderr, "Warning: ihex line %d: parse error\n", line_num);
            continue;
        }

        uint16_t addr = (addr_hi << 8) | addr_lo;

        switch (rec_type) {
        case 0x00: /* Data record */
            for (int i = 0; i < byte_count; i++) {
                int b = hex_byte(p); p += 2;
                if (b < 0) break;
                uint32_t full_addr = base_addr + addr + i;
                if (full_addr < buf_size)
                    buf[full_addr] = b;
                if (full_addr + 1 > max_addr)
                    max_addr = full_addr + 1;
            }
            break;
        case 0x01: /* End of file */
            goto done;
        case 0x02: /* Extended segment address */
            {
                int seg_hi = hex_byte(p); p += 2;
                int seg_lo = hex_byte(p); p += 2;
                if (seg_hi >= 0 && seg_lo >= 0)
                    base_addr = ((seg_hi << 8) | seg_lo) << 4;
            }
            break;
        case 0x04: /* Extended linear address */
            {
                int lin_hi = hex_byte(p); p += 2;
                int lin_lo = hex_byte(p); p += 2;
                if (lin_hi >= 0 && lin_lo >= 0)
                    base_addr = ((lin_hi << 8) | lin_lo) << 16;
            }
            break;
        default:
            break;
        }
    }

done:
    fclose(f);
    *data_len = max_addr;
    return 0;
}

int file_write_intel_hex(const char *path, uint8_t *buf, uint32_t data_len)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s' for writing\n", path);
        return -1;
    }

    uint32_t ext_addr = 0;
    int bytes_per_line = 16;

    for (uint32_t offset = 0; offset < data_len; offset += bytes_per_line) {
        /* Extended linear address record if needed */
        uint32_t cur_ext = offset & 0xFFFF0000;
        if (cur_ext != ext_addr) {
            ext_addr = cur_ext;
            uint8_t hi = (ext_addr >> 24) & 0xFF;
            uint8_t lo = (ext_addr >> 16) & 0xFF;
            uint8_t checksum = 0x02 + 0x04 + hi + lo;
            checksum = (~checksum) + 1;
            fprintf(f, ":02000004%02X%02X%02X\n", hi, lo, checksum);
        }

        int count = data_len - offset;
        if (count > bytes_per_line) count = bytes_per_line;

        uint16_t local_addr = offset & 0xFFFF;
        uint8_t checksum = count + (local_addr >> 8) + (local_addr & 0xFF) + 0x00;

        fprintf(f, ":%02X%04X00", count, local_addr);
        for (int i = 0; i < count; i++) {
            fprintf(f, "%02X", buf[offset + i]);
            checksum += buf[offset + i];
        }
        checksum = (~checksum) + 1;
        fprintf(f, "%02X\n", checksum);
    }

    /* End of file record */
    fprintf(f, ":00000001FF\n");
    fclose(f);
    return 0;
}

/* SREC (Motorola S-Record) parser */
int file_read_srec(const char *path, uint8_t *buf, uint32_t buf_size,
                   uint32_t *data_len)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s' for reading\n", path);
        return -1;
    }

    char line[600];
    uint32_t max_addr = 0;

    memset(buf, 0xFF, buf_size);

    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p && isspace(*p)) p++;
        if (*p != 'S' && *p != 's') continue;
        p++;

        int rec_type = *p - '0';
        p++;

        int byte_count = hex_byte(p); p += 2;
        if (byte_count < 0) continue;

        uint32_t addr = 0;
        int addr_bytes = 0;

        switch (rec_type) {
        case 1: addr_bytes = 2; break; /* S1: 16-bit addr */
        case 2: addr_bytes = 3; break; /* S2: 24-bit addr */
        case 3: addr_bytes = 4; break; /* S3: 32-bit addr */
        default: continue;
        }

        for (int i = 0; i < addr_bytes; i++) {
            int b = hex_byte(p); p += 2;
            if (b < 0) goto next_line;
            addr = (addr << 8) | b;
        }

        int data_bytes = byte_count - addr_bytes - 1; /* -1 for checksum */
        for (int i = 0; i < data_bytes; i++) {
            int b = hex_byte(p); p += 2;
            if (b < 0) break;
            uint32_t full_addr = addr + i;
            if (full_addr < buf_size)
                buf[full_addr] = b;
            if (full_addr + 1 > max_addr)
                max_addr = full_addr + 1;
        }
next_line:;
    }

    fclose(f);
    *data_len = max_addr;
    return 0;
}

int file_write_srec(const char *path, uint8_t *buf, uint32_t data_len)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s' for writing\n", path);
        return -1;
    }

    int bytes_per_line = 16;

    /* S0 header */
    fprintf(f, "S0030000FC\n");

    for (uint32_t offset = 0; offset < data_len; offset += bytes_per_line) {
        int count = data_len - offset;
        if (count > bytes_per_line) count = bytes_per_line;

        /* Use S3 (32-bit address) records */
        int byte_count = count + 4 + 1; /* data + 4-byte addr + checksum */
        uint8_t checksum = byte_count;
        checksum += (offset >> 24) & 0xFF;
        checksum += (offset >> 16) & 0xFF;
        checksum += (offset >> 8) & 0xFF;
        checksum += offset & 0xFF;

        fprintf(f, "S3%02X%08X", byte_count, offset);
        for (int i = 0; i < count; i++) {
            fprintf(f, "%02X", buf[offset + i]);
            checksum += buf[offset + i];
        }
        checksum = ~checksum;
        fprintf(f, "%02X\n", checksum);
    }

    /* S7 end record */
    fprintf(f, "S70500000000FA\n");
    fclose(f);
    return 0;
}

/* Detect file format from content */
file_format_t file_detect_format(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return FMT_BIN;

    int c = fgetc(f);
    fclose(f);

    if (c == ':')
        return FMT_IHEX;
    if (c == 'S' || c == 's')
        return FMT_SREC;

    return FMT_BIN;
}

/* Unified read/write wrappers */
int file_read_buf(const char *path, file_format_t fmt, uint8_t *buf,
                  uint32_t buf_size, uint32_t *data_len)
{
    if (fmt == FMT_AUTO)
        fmt = file_detect_format(path);

    switch (fmt) {
    case FMT_IHEX: return file_read_intel_hex(path, buf, buf_size, data_len);
    case FMT_SREC: return file_read_srec(path, buf, buf_size, data_len);
    default:       return file_read_bin(path, buf, buf_size, data_len);
    }
}

int file_write_buf(const char *path, file_format_t fmt, uint8_t *buf,
                   uint32_t data_len)
{
    switch (fmt) {
    case FMT_IHEX: return file_write_intel_hex(path, buf, data_len);
    case FMT_SREC: return file_write_srec(path, buf, data_len);
    default:       return file_write_bin(path, buf, data_len);
    }
}
