/*
 * nand_dump.c - Full NAND dump tool for XGecu T76
 *
 * Protocol (reverse engineered from USB capture + address testing):
 * - NAND_INIT (0x02) + 128-byte BEGIN_TRANS required before each read
 * - READ_DATA (0x10) with page number in msg[4..7] LE32
 * - Each read returns 8448 bytes (4 pages of 2112 bytes each)
 * - Must reopen USB handle between reads (device hangs otherwise)
 * - FPGA bitstream persists across USB reopens
 *
 * MT29F2G08: 131072 pages, 32768 read blocks of 4 pages each
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
static char devPath[512] = {0};

static int usb_find(void) {
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
    strncpy(devPath, dd->DevicePath, sizeof(devPath)-1);
    free(dd); SetupDiDestroyDeviceInfoList(di);
    return 0;
}
static int usb_open(void) {
    hDev = CreateFileA(devPath, GENERIC_READ|GENERIC_WRITE,
                       FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED, NULL);
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

/* Read one block at page_num. Reopens USB each time. */
static int read_block(UCHAR *buf, UINT page_num, UINT size, int need_bs,
                      UCHAR *bs, UINT bslen) {
    UCHAR m[512], r[64];
    if (usb_open()) return -1;

    if (need_bs) {
        memset(m,0,8); snd(1,m,8); rcv(0x81,r,64);
        memset(m,0,8);m[0]=0x26;m[1]=0xAF;m[4]=0xEE;m[5]=0xDD;m[6]=0x55;m[7]=0xAA;
        snd(1,m,8); rcv(0x81,r,64);
        memset(m,0,8);m[0]=0x26;le16(&m[2],512);le32(&m[4],bslen);
        snd(1,m,8); rcv(0x81,r,64);
        for(UINT i=0;i<bslen;i+=504){UINT bsz=(i+504<=bslen)?504:(bslen-i);
            memset(m,0,512);m[0]=0x26;m[1]=0x01;le16(&m[2],(UINT)bsz);
            memcpy(&m[8],&bs[i],bsz);snd(1,m,512);}
        memset(m,0,8);m[0]=0x26;m[1]=0x02;snd(1,m,8);rcv(0x81,r,64);
        if(r[1]){usb_close();return -2;}
        Sleep(30);
    }

    snd(0x01, nand_init, 64);
    snd(0x01, begin_trans, 128);
    memset(m,0,8); m[0]=0x39; snd(1,m,8); rcv(0x81,r,32);

    memset(m,0,16); m[0]=0x10; le16(&m[2],size); le32(&m[4],page_num);
    snd(0x01, m, 16);
    int n = rcv(0x82, buf, size + 16);

    memset(m,0,4); m[0]=0x04; snd(1,m,4);
    usb_close();
    return n;
}

int main(int argc, char **argv) {
    const char *outfile = (argc > 1) ? argv[1] : "nand_dump.bin";
    int strip_oob = (argc > 2 && strcmp(argv[2], "--no-oob") == 0);

    printf("=== T76 NAND Full Dump ===\n\n");

    if (usb_find()) { printf("T76 not found\n"); return 1; }

    /* Load bitstream */
    FILE *f = fopen("algoT76\\T7_Nand_84.alg", "rb");
    if (!f) f = fopen("algoT76/T7_Nand_84.alg", "rb");
    if (!f) { printf("No alg file\n"); return 1; }
    fseek(f,0,SEEK_END); long fsz=ftell(f); fseek(f,0,SEEK_SET);
    UCHAR *raw=malloc(fsz); fread(raw,1,fsz,f); fclose(f);
    UINT asz=raw[4]|(raw[5]<<8)|(raw[6]<<16)|(raw[7]<<24);
    UCHAR *bs=calloc(1,asz);
    unsigned short *out=(unsigned short*)bs,*in=(unsigned short*)&raw[4096];
    unsigned short *ie=(unsigned short*)&raw[fsz],*oe=(unsigned short*)(bs+asz);
    while(in<ie&&out<oe){unsigned short v=*in++;if(v)*out++=v;else{if(in>=ie)break;
        unsigned short c=*in++;if(!c)break;if(out+c>oe)c=(unsigned short)(oe-out);
        memset(out,0,c*2);out+=c;}}
    free(raw);

    /* Read block 0 with bitstream upload */
    UINT rbsz = 8448; /* 4 pages * 2112 */
    UINT pages_per_read = 4;
    UINT total_pages = 131072;
    UINT total_blocks = total_pages / pages_per_read; /* 32768 */

    UCHAR *buf = calloc(1, rbsz + 16);

    printf("Uploading bitstream + reading block 0...\n");
    int n = read_block(buf, 0, rbsz, 1, bs, asz);
    if (n < 16) { printf("FAILED (got %d)\n", n); free(bs); free(buf); return 1; }
    printf("Block 0: ");
    for(int i=16;i<32&&i<n;i++) printf("%02X ",buf[i]);
    printf("\n");

    /* Verify chip responds */
    if (buf[16]==0x00 && buf[17]==0x00 && buf[18]==0x00 && buf[19]==0x00) {
        printf("WARNING: Block 0 is all zeros. Check chip contact.\n");
    }

    /* Open output file */
    FILE *fout = fopen(outfile, "wb");
    if (!fout) { printf("Cannot create %s\n", outfile); free(bs); free(buf); return 1; }

    /* Write first block */
    fwrite(buf + 16, 1, n - 16, fout);

    printf("\nDumping %u blocks (%u pages, %u MB)...\n",
           total_blocks, total_pages, (UINT)((UINT64)total_pages * 2112 / 1024 / 1024));
    printf("Output: %s%s\n\n", outfile, strip_oob ? " (data only, no OOB)" : " (with OOB/spare)");

    DWORD t0 = GetTickCount();
    UINT errors = 0;
    UINT retries = 0;

    for (UINT blk = 1; blk < total_blocks; blk++) {
        UINT page = blk * pages_per_read;

        n = read_block(buf, page, rbsz, 0, NULL, 0);

        if (n < 16) {
            /* Retry with bitstream reload */
            retries++;
            n = read_block(buf, page, rbsz, 1, bs, asz);
        }

        if (n < 16) {
            errors++;
            if (errors > 50) {
                printf("\n\nToo many errors (%u). Stopping at block %u.\n", errors, blk);
                break;
            }
            memset(buf+16, 0xFF, rbsz);
            n = rbsz + 16;
        }

        UINT dlen = n - 16;
        if (dlen > rbsz) dlen = rbsz;
        fwrite(buf + 16, 1, dlen, fout);

        /* Progress */
        if (blk % 32 == 0 || blk == total_blocks - 1) {
            DWORD elapsed = (GetTickCount() - t0) / 1000;
            UINT pct = ((blk+1) * 100) / total_blocks;
            UINT mb = (UINT)(((UINT64)(blk+1) * rbsz) / (1024*1024));
            UINT eta = elapsed > 0 ? (elapsed * (total_blocks - blk - 1)) / (blk + 1) : 0;
            printf("\r  [");
            for(int i=0;i<40;i++) printf(i<(int)(pct*40/100)?"#":"-");
            printf("] %u%% %uMB %us ETA:%us err:%u  ",
                   pct, mb, (UINT)elapsed, eta, errors);
            fflush(stdout);
        }
    }

    printf("\n\n");
    fclose(fout);

    /* Stats */
    fout = fopen(outfile, "rb");
    if (fout) {
        fseek(fout, 0, SEEK_END);
        long size = ftell(fout);
        fclose(fout);
        printf("Written: %ld bytes (%ld MB)\n", size, size/(1024*1024));
    }

    DWORD total_time = (GetTickCount() - t0) / 1000;
    printf("Time: %u seconds (%u minutes)\n", (UINT)total_time, (UINT)total_time/60);
    printf("Errors: %u, Retries: %u\n", errors, retries);

    if (errors == 0)
        printf("\nDump completed successfully!\n");
    else
        printf("\nDump completed with %u errors (bad blocks filled with 0xFF)\n", errors);

    free(bs); free(buf);
    printf("\nDone.\n");
    return 0;
}
