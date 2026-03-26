/*
 * nand_addr_test.c - Test different address formats for NAND READ_DATA
 * Reads 5 blocks using different address encoding to find which one
 * actually changes the read position.
 */
#include <windows.h>
#include <winusb.h>
#include <setupapi.h>
#include <initguid.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DEFINE_GUID(T76_GUID, 0x015DE341, 0x91CC, 0x8286,
            0x39, 0x64, 0x1A, 0x00, 0x6B, 0xC1, 0xF0, 0x0F);
static HANDLE hDev = INVALID_HANDLE_VALUE;
static WINUSB_INTERFACE_HANDLE hUSB = NULL;

static int usb_open(void) {
    HDEVINFO di = SetupDiGetClassDevs(&T76_GUID, NULL, NULL, DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if (di == INVALID_HANDLE_VALUE) return -1;
    SP_DEVICE_INTERFACE_DATA id = {.cbSize = sizeof(id)};
    if (!SetupDiEnumDeviceInterfaces(di, NULL, &T76_GUID, 0, &id)) {
        SetupDiDestroyDeviceInfoList(di); return -1; }
    DWORD sz = 0;
    SetupDiGetDeviceInterfaceDetail(di, &id, NULL, 0, &sz, NULL);
    SP_DEVICE_INTERFACE_DETAIL_DATA_A *dd = malloc(sz);
    dd->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
    SetupDiGetDeviceInterfaceDetailA(di, &id, dd, sz, NULL, NULL);
    hDev = CreateFileA(dd->DevicePath, GENERIC_READ|GENERIC_WRITE,
                       FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED, NULL);
    free(dd); SetupDiDestroyDeviceInfoList(di);
    if (hDev == INVALID_HANDLE_VALUE) return -1;
    if (!WinUsb_Initialize(hDev, &hUSB)) { CloseHandle(hDev); hDev=INVALID_HANDLE_VALUE; return -1; }
    ULONG tmo = 10000;
    WinUsb_SetPipePolicy(hUSB, 0x81, PIPE_TRANSFER_TIMEOUT, 4, &tmo);
    WinUsb_SetPipePolicy(hUSB, 0x82, PIPE_TRANSFER_TIMEOUT, 4, &tmo);
    return 0;
}
static void usb_close(void) {
    if (hUSB) { WinUsb_Free(hUSB); hUSB=NULL; }
    if (hDev != INVALID_HANDLE_VALUE) { CloseHandle(hDev); hDev=INVALID_HANDLE_VALUE; }
}
static int snd(UCHAR ep, UCHAR *d, int len) {
    ULONG t; return WinUsb_WritePipe(hUSB, ep, d, len, &t, NULL) ? (int)t : -1; }
static int rcv(UCHAR ep, UCHAR *d, int len) {
    ULONG t=0; if (!WinUsb_ReadPipe(hUSB, ep, d, len, &t, NULL)) return -1; return (int)t; }
static void le16(UCHAR *b, UINT v) { b[0]=v&0xFF; b[1]=(v>>8)&0xFF; }
static void le32(UCHAR *b, UINT v) { b[0]=v&0xFF; b[1]=(v>>8)&0xFF; b[2]=(v>>16)&0xFF; b[3]=(v>>24)&0xFF; }

static UCHAR nand_init[64] = {
    0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x08,0x00,0x08,0x40,0x00,
    0x01,0x00,0x01,0x00,0x03,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x01,0x00,0x3B,0x4F,0x15,0x27,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static UCHAR begin_trans[128] = {
    0x03,0x2D,0x00,0x00,0x06,0x00,0xA0,0x6F,0x00,0x00,0x00,0x08,0x40,0x00,0x20,0x00,
    0x00,0x10,0x02,0x00,0x00,0x06,0x00,0x00,0x03,0x00,0x00,0x00,0x03,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x21,0x00,0x00,
    0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x78,0x08,0x00,0x00,0x00,0x00,0x00,0x84,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x2F,0x27,0x09,0x0B,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static int read_block_with_cmd(UCHAR *buf, UCHAR *cmd, int cmdlen, UINT readsize,
                               UCHAR *bs, UINT bslen, int need_bs) {
    UCHAR m[512], r[64];
    if (usb_open()) return -1;

    if (need_bs) {
        memset(m,0,8); snd(1,m,8); rcv(0x81,r,64);
        memset(m,0,8);m[0]=0x26;m[1]=0xAF;m[4]=0xEE;m[5]=0xDD;m[6]=0x55;m[7]=0xAA;
        snd(1,m,8); rcv(0x81,r,64);
        memset(m,0,8);m[0]=0x26;le16(&m[2],512);le32(&m[4],bslen);
        snd(1,m,8); rcv(0x81,r,64);
        for(UINT i=0;i<bslen;i+=504){UINT bsz=(i+504<=bslen)?504:(bslen-i);
            memset(m,0,512);m[0]=0x26;m[1]=0x01;le16(&m[2],(UINT)bsz);memcpy(&m[8],&bs[i],bsz);snd(1,m,512);}
        memset(m,0,8);m[0]=0x26;m[1]=0x02;snd(1,m,8);rcv(0x81,r,64);
        Sleep(50);
    }

    snd(0x01, nand_init, 64);
    snd(0x01, begin_trans, 128);
    memset(m,0,8); m[0]=0x39; snd(1,m,8); rcv(0x81,r,32);

    snd(0x01, cmd, cmdlen);
    int n = rcv(0x82, buf, readsize + 16);

    memset(m,0,4); m[0]=0x04; snd(1,m,4);
    usb_close();
    return n;
}

/* Compute a simple hash of first 256 bytes to detect changes */
static UINT hash256(UCHAR *d, int len) {
    UINT h = 0x811c9dc5;
    for (int i=0; i<256 && i<len; i++) { h ^= d[i]; h *= 0x01000193; }
    return h;
}

int main(void) {
    printf("=== NAND Address Format Test ===\n\n");

    /* Load bitstream */
    FILE *f = fopen("algoT76\\T7_Nand_84.alg", "rb");
    if (!f) f = fopen("algoT76/T7_Nand_84.alg", "rb");
    if (!f) { printf("No alg\n"); return 1; }
    fseek(f,0,SEEK_END); long fsz=ftell(f); fseek(f,0,SEEK_SET);
    UCHAR *raw=malloc(fsz); fread(raw,1,fsz,f); fclose(f);
    UINT asz=raw[4]|(raw[5]<<8)|(raw[6]<<16)|(raw[7]<<24);
    UCHAR *bs=calloc(1,asz);
    unsigned short *out=(unsigned short*)bs,*in=(unsigned short*)&raw[4096];
    unsigned short *ie=(unsigned short*)&raw[fsz],*oe=(unsigned short*)(bs+asz);
    while(in<ie&&out<oe){unsigned short v=*in++;if(v)*out++=v;else{if(in>=ie)break;unsigned short c=*in++;if(!c)break;if(out+c>oe)c=(unsigned short)(oe-out);memset(out,0,c*2);out+=c;}}
    free(raw);

    UINT rbsz = 8448;
    UCHAR *buf = calloc(1, rbsz + 16);
    UCHAR cmd[16];
    int n;
    UINT hash0;

    /* Reference: read block 0 */
    printf("Reference: block 0\n");
    memset(cmd,0,16); cmd[0]=0x10; le16(&cmd[2],rbsz);
    n = read_block_with_cmd(buf, cmd, 16, rbsz, bs, asz, 1);
    printf("  Got %d bytes, first 16: ", n);
    if (n>16) for(int i=16;i<32;i++) printf("%02X ",buf[i]);
    printf("\n");
    hash0 = (n>16) ? hash256(buf+16, n-16) : 0;
    printf("  Hash: 0x%08X\n\n", hash0);

    /* Test different address formats */
    struct {
        const char *desc;
        UCHAR cmd[16];
        int cmdlen;
    } tests[] = {
        /* Test 1: addr in msg[4..7] as page number (page 64 = block 1) */
        {"addr[4..7] = page 64 (block 1)", {0x10, 0x00, 0x00, 0x21, 0x40, 0x00, 0x00, 0x00}, 16},
        /* Test 2: addr in msg[4..7] as byte offset 0x21000 (block 1 * 8448) */
        {"addr[4..7] = byte 0x2100 * 4", {0x10, 0x00, 0x00, 0x21, 0x00, 0x84, 0x00, 0x00}, 16},
        /* Test 3: addr in msg[8..11] instead of msg[4..7] */
        {"addr[8..11] = 1 (block count field?)", {0x10, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00}, 16},
        /* Test 4: READ_CODE (0x0D) with block_count=2 - maybe streams 2 blocks */
        {"READ_CODE blk_count=2", {0x0D, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00}, 16},
        /* Test 5: msg[1] = page number */
        {"msg[1] = 64 (page num)", {0x10, 0x40, 0x00, 0x21, 0x00, 0x00, 0x00, 0x00}, 16},
        /* Test 6: addr as NAND row address (page<<16 | column) */
        {"addr = NAND row 0x40 (page 64)", {0x10, 0x00, 0x00, 0x21, 0x00, 0x00, 0x40, 0x00}, 16},
        /* Test 7: addr[4..7] = 1 (simplest: block number) */
        {"addr[4..7] = 1 (block number)", {0x10, 0x00, 0x00, 0x21, 0x01, 0x00, 0x00, 0x00}, 16},
        /* Test 8: size field different - maybe size encodes page? */
        {"size = 8448*2 (two blocks)", {0x10, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x00}, 16},
        /* Test 9: addr[4..7] = page 128 (block 2) */
        {"addr[4..7] = 128 (page, blk 2)", {0x10, 0x00, 0x00, 0x21, 0x80, 0x00, 0x00, 0x00}, 16},
        /* Test 10: READ_DATA with msg[4..7] = page_size * page_num */
        {"addr = 2048 (one page offset)", {0x10, 0x00, 0x00, 0x21, 0x00, 0x08, 0x00, 0x00}, 16},
    };
    int num_tests = 10;

    for (int t = 0; t < num_tests; t++) {
        printf("Test %d: %s\n", t+1, tests[t].desc);
        printf("  CMD: ");
        for (int i=0; i<tests[t].cmdlen && i<16; i++) printf("%02X ", tests[t].cmd[i]);
        printf("\n");

        n = read_block_with_cmd(buf, tests[t].cmd, tests[t].cmdlen, rbsz, bs, asz, 0);
        if (n < 16) {
            printf("  FAIL (got %d, err=%lu)\n\n", n, GetLastError());
            continue;
        }

        UINT h = hash256(buf+16, n-16);
        printf("  Got %d bytes, first 16: ", n);
        for(int i=16;i<32&&i<n;i++) printf("%02X ",buf[i]);
        printf("\n  Hash: 0x%08X %s\n\n", h, h==hash0 ? "(SAME as block 0)" : "*** DIFFERENT! ***");
    }

    free(bs); free(buf);
    printf("Done.\n");
    return 0;
}
