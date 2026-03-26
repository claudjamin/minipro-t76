/*
 * nand_scan.c - Quick NAND content scanner
 * Reads blocks at intervals across the chip to find data regions fast.
 * Uses the proven NAND_INIT + 128-byte BEGIN_TRANS protocol.
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
    if (!WinUsb_Initialize(hDev, &hUSB)) { CloseHandle(hDev); return -1; }
    ULONG tmo = 30000;
    WinUsb_SetPipePolicy(hUSB, 0x81, PIPE_TRANSFER_TIMEOUT, 4, &tmo);
    WinUsb_SetPipePolicy(hUSB, 0x82, PIPE_TRANSFER_TIMEOUT, 4, &tmo);
    return 0;
}
static void usb_close(void) {
    if (hUSB) WinUsb_Free(hUSB);
    if (hDev != INVALID_HANDLE_VALUE) CloseHandle(hDev); }
static int snd(UCHAR ep, UCHAR *d, int len) {
    ULONG t; return WinUsb_WritePipe(hUSB, ep, d, len, &t, NULL) ? (int)t : -1; }
static int rcv(UCHAR ep, UCHAR *d, int len) {
    ULONG t=0; if (!WinUsb_ReadPipe(hUSB, ep, d, len, &t, NULL)) return -1; return (int)t; }
static void le16(UCHAR *b, UINT v) { b[0]=v&0xFF; b[1]=(v>>8)&0xFF; }
static void le32(UCHAR *b, UINT v) { b[0]=v&0xFF; b[1]=(v>>8)&0xFF; b[2]=(v>>16)&0xFF; b[3]=(v>>24)&0xFF; }

static UCHAR nand_init[64] = {
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x08, 0x00, 0x08, 0x40, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x3B, 0x4F, 0x15, 0x27,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static UCHAR begin_trans[128] = {
    0x03, 0x2D, 0x00, 0x00, 0x06, 0x00, 0xA0, 0x6F,
    0x00, 0x00, 0x00, 0x08, 0x40, 0x00, 0x20, 0x00,
    0x00, 0x10, 0x02, 0x00, 0x00, 0x06, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x78, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x84,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x2F, 0x27, 0x09, 0x0B, 0x00, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void do_begin(UCHAR *m, UCHAR *r) {
    snd(0x01, nand_init, 64);
    snd(0x01, begin_trans, 128);
    memset(m,0,8); m[0]=0x39; snd(1,m,8); rcv(0x81,r,32);
}

static void do_end(UCHAR *m) {
    memset(m,0,4); m[0]=0x04; snd(1,m,4);
}

/* Read one block at a given address. Returns bytes read or -1. */
static int read_block(UCHAR *m, UCHAR *r, UCHAR *buf, UINT addr, UINT size) {
    do_end(m);
    do_begin(m, r);
    memset(m,0,16); m[0]=0x10; le16(&m[2],size); le32(&m[4],addr);
    snd(0x01, m, 16);
    return rcv(0x82, buf, size + 16);
}

int main(int argc, char **argv) {
    UCHAR m[512], r[64];
    const char *outfile = (argc > 1) ? argv[1] : NULL;

    printf("=== T76 NAND Scanner ===\n\n");

    if (usb_open()) { printf("T76 not found\n"); return 1; }

    /* Device info */
    memset(m,0,8); snd(1,m,8); rcv(0x81,r,64);
    printf("FW: 0x%04X  Code: %.8s\n", r[4]|(r[5]<<8), &r[24]);

    /* Load bitstream */
    FILE *f = fopen("algoT76\\T7_Nand_84.alg", "rb");
    if (!f) f = fopen("algoT76/T7_Nand_84.alg", "rb");
    if (!f) { printf("No alg file\n"); usb_close(); return 1; }
    fseek(f,0,SEEK_END); long fsz=ftell(f); fseek(f,0,SEEK_SET);
    UCHAR *raw=malloc(fsz); fread(raw,1,fsz,f); fclose(f);
    UINT asz=raw[4]|(raw[5]<<8)|(raw[6]<<16)|(raw[7]<<24);
    UCHAR *bs=calloc(1,asz);
    unsigned short *out=(unsigned short*)bs,*in=(unsigned short*)&raw[4096];
    unsigned short *ie=(unsigned short*)&raw[fsz],*oe=(unsigned short*)(bs+asz);
    while(in<ie&&out<oe){unsigned short v=*in++;if(v)*out++=v;else{if(in>=ie)break;unsigned short c=*in++;if(!c)break;if(out+c>oe)c=(unsigned short)(oe-out);memset(out,0,c*2);out+=c;}}
    free(raw);

    memset(m,0,8);m[0]=0x26;m[1]=0xAF;m[4]=0xEE;m[5]=0xDD;m[6]=0x55;m[7]=0xAA;
    snd(1,m,8); rcv(0x81,r,64);
    memset(m,0,8);m[0]=0x26;le16(&m[2],512);le32(&m[4],asz);
    snd(1,m,8); rcv(0x81,r,64);
    if(r[1]){printf("BS fail\n");free(bs);usb_close();return 1;}
    for(UINT i=0;i<asz;i+=504){UINT bsz=(i+504<=asz)?504:(asz-i);memset(m,0,512);m[0]=0x26;m[1]=0x01;le16(&m[2],(UINT)bsz);memcpy(&m[8],&bs[i],bsz);snd(1,m,512);}
    memset(m,0,8);m[0]=0x26;m[1]=0x02;snd(1,m,8);rcv(0x81,r,64);
    free(bs);
    if(r[1]){printf("BS fail\n");usb_close();return 1;}
    printf("Bitstream OK\n\n");
    Sleep(50);

    /* Verify chip ID */
    do_begin(m, r);
    memset(m,0,8); m[0]=0x05; snd(1,m,8); rcv(0x81,r,32);
    printf("Chip ID: %02X %02X %02X %02X %02X", r[2],r[3],r[4],r[5],r[6]);
    if (r[2]==0x2C) printf(" (Micron OK)\n\n");
    else if (r[2]==0x00) { printf(" (NO RESPONSE)\n"); usb_close(); return 1; }
    else printf("\n\n");

    UINT rbsz = 8448;
    UINT total = 276824064;
    UINT total_blocks = total / rbsz; /* 32768 */
    UCHAR *buf = calloc(1, rbsz + 16);

    /* SCAN MODE: read blocks at intervals to find data regions */
    UINT step = 256; /* read every 256th block = ~2MB intervals */
    UINT scan_count = total_blocks / step;

    printf("=== Scanning %u blocks (every %uth, %u samples across %uMB) ===\n\n",
           scan_count, step, scan_count, total / (1024*1024));
    printf("  Block    Address     Status   First 16 data bytes\n");
    printf("  -----    -------     ------   -------------------\n");

    UINT data_regions = 0;
    UINT blank_regions = 0;
    UINT fail_regions = 0;
    UINT first_data_block = 0xFFFFFFFF;
    UINT last_data_block = 0;

    FILE *fout = NULL;
    if (outfile) {
        fout = fopen(outfile, "wb");
        if (!fout) printf("Warning: cannot create %s\n", outfile);
    }

    for (UINT i = 0; i < scan_count; i++) {
        UINT blk = i * step;
        UINT addr = blk * rbsz;

        int n = read_block(m, r, buf, addr, rbsz);

        if (n < 16) {
            printf("  %5u    0x%08X  FAIL     (err=%lu)\n", blk, addr, GetLastError());
            fail_regions++;
            continue;
        }

        /* Analyze content (skip 16-byte header) */
        UCHAR *data = buf + 16;
        UINT dlen = n - 16;
        int all_ff = 1, all_00 = 1, nontrivial = 0;
        for (UINT j = 0; j < dlen && j < 2048; j++) {
            if (data[j] != 0xFF) all_ff = 0;
            if (data[j] != 0x00) all_00 = 0;
            if (data[j] != 0xFF && data[j] != 0x00) nontrivial++;
        }

        char status[16];
        if (all_ff) { strcpy(status, "BLANK"); blank_regions++; }
        else if (all_00) { strcpy(status, "ZEROS"); fail_regions++; }
        else { strcpy(status, "DATA!"); data_regions++;
               if (blk < first_data_block) first_data_block = blk;
               if (blk > last_data_block) last_data_block = blk; }

        printf("  %5u    0x%08X  %-5s    ", blk, addr, status);
        for (int j = 0; j < 16 && j < (int)dlen; j++) printf("%02X ", data[j]);
        if (nontrivial > 0) printf(" (%d non-trivial)", nontrivial);
        printf("\n");

        /* Save data blocks to file */
        if (fout && !all_ff && !all_00) {
            /* Write block number as header */
            UINT hdr[2] = { blk, addr };
            fwrite(hdr, 1, 8, fout);
            fwrite(data, 1, dlen, fout);
        }
    }

    printf("\n=== Scan Summary ===\n");
    printf("  Data regions:  %u\n", data_regions);
    printf("  Blank (0xFF):  %u\n", blank_regions);
    printf("  Failed/zeros:  %u\n", fail_regions);
    if (first_data_block != 0xFFFFFFFF)
        printf("  Data range:    blocks %u - %u (addr 0x%08X - 0x%08X)\n",
               first_data_block, last_data_block,
               first_data_block * rbsz, (last_data_block + 1) * rbsz);
    else
        printf("  No data found!\n");

    if (fout) {
        fclose(fout);
        printf("  Saved data blocks to %s\n", outfile);
    }

    /* End */
    do_end(m);
    free(buf);
    usb_close();
    printf("\nDone.\n");
    return 0;
}
