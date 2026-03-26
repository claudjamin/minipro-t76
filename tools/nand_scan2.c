/*
 * nand_scan2.c - NAND scanner with full USB re-init per block
 * Closes and reopens WinUSB handle between each read.
 * Skips bitstream reload (FPGA retains state).
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
    if (hUSB) { WinUsb_Free(hUSB); hUSB = NULL; }
    if (hDev != INVALID_HANDLE_VALUE) { CloseHandle(hDev); hDev = INVALID_HANDLE_VALUE; }
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

/* Read one block: reopen USB, send NAND_INIT + BEGIN + READ_DATA, close */
static int read_one_block(UCHAR *buf, UINT addr, UINT size, int need_bitstream,
                          UCHAR *bs_data, UINT bs_size) {
    UCHAR m[512], r[64];

    if (usb_open()) return -1;

    if (need_bitstream) {
        /* Device info first */
        memset(m,0,8); snd(1,m,8); rcv(0x81,r,64);

        /* FPGA Reset */
        memset(m,0,8);m[0]=0x26;m[1]=0xAF;m[4]=0xEE;m[5]=0xDD;m[6]=0x55;m[7]=0xAA;
        snd(1,m,8); rcv(0x81,r,64);

        /* Upload bitstream */
        memset(m,0,8);m[0]=0x26;le16(&m[2],512);le32(&m[4],bs_size);
        snd(1,m,8); rcv(0x81,r,64);
        for(UINT i=0;i<bs_size;i+=504){
            UINT bsz=(i+504<=bs_size)?504:(bs_size-i);
            memset(m,0,512);m[0]=0x26;m[1]=0x01;le16(&m[2],(UINT)bsz);
            memcpy(&m[8],&bs_data[i],bsz);snd(1,m,512);
        }
        memset(m,0,8);m[0]=0x26;m[1]=0x02;snd(1,m,8);rcv(0x81,r,64);
        if(r[1]){usb_close();return -2;}
        Sleep(50);
    }

    /* NAND_INIT + BEGIN_TRANS */
    snd(0x01, nand_init, 64);
    snd(0x01, begin_trans, 128);
    memset(m,0,8); m[0]=0x39; snd(1,m,8); rcv(0x81,r,32);

    /* READ_DATA at address */
    memset(m,0,16); m[0]=0x10; le16(&m[2],size); le32(&m[4],addr);
    snd(0x01, m, 16);

    int n = rcv(0x82, buf, size + 16);

    /* END_TRANS */
    memset(m,0,4); m[0]=0x04; snd(1,m,4);

    usb_close();
    return n;
}

int main(int argc, char **argv) {
    const char *outfile = (argc > 1) ? argv[1] : NULL;

    printf("=== T76 NAND Scanner v2 (USB re-init per block) ===\n\n");

    /* Load bitstream into memory */
    FILE *f = fopen("algoT76\\T7_Nand_84.alg", "rb");
    if (!f) f = fopen("algoT76/T7_Nand_84.alg", "rb");
    if (!f) { printf("No alg file\n"); return 1; }
    fseek(f,0,SEEK_END); long fsz=ftell(f); fseek(f,0,SEEK_SET);
    UCHAR *raw=malloc(fsz); fread(raw,1,fsz,f); fclose(f);
    UINT asz=raw[4]|(raw[5]<<8)|(raw[6]<<16)|(raw[7]<<24);
    UCHAR *bs=calloc(1,asz);
    unsigned short *out=(unsigned short*)bs,*in=(unsigned short*)&raw[4096];
    unsigned short *ie=(unsigned short*)&raw[fsz],*oe=(unsigned short*)(bs+asz);
    while(in<ie&&out<oe){unsigned short v=*in++;if(v)*out++=v;else{if(in>=ie)break;unsigned short c=*in++;if(!c)break;if(out+c>oe)c=(unsigned short)(oe-out);memset(out,0,c*2);out+=c;}}
    free(raw);
    printf("Bitstream ready (%u bytes)\n\n", asz);

    UINT rbsz = 8448;
    UINT total = 276824064;
    UINT total_blocks = total / rbsz;
    UCHAR *buf = calloc(1, rbsz + 16);

    /* First read: needs bitstream upload */
    printf("Reading block 0 (with bitstream upload)...\n");
    int n = read_one_block(buf, 0, rbsz, 1, bs, asz);
    if (n < 16) {
        printf("FAILED! Cannot read block 0 (got %d)\n", n);
        printf("Check chip contact.\n");
        free(bs); free(buf); return 1;
    }
    printf("Block 0 OK! First bytes: ");
    for (int i=16; i<32 && i<n; i++) printf("%02X ", buf[i]);
    printf("\n\n");

    /* Scan at intervals - bitstream persists in FPGA */
    UINT step = 512;
    UINT samples = total_blocks / step;
    printf("Scanning %u samples (every %u blocks, ~%uMB apart)...\n\n", samples, step, (step*rbsz)/(1024*1024));
    printf("  Block    Address     Status   First 16 data bytes\n");
    printf("  -----    -------     ------   -------------------\n");

    FILE *fout = outfile ? fopen(outfile, "wb") : NULL;
    UINT data_count=0, blank_count=0, fail_count=0;

    for (UINT i = 0; i < samples; i++) {
        UINT blk = i * step;
        UINT addr = blk * rbsz;

        /* No bitstream reload - FPGA keeps it */
        n = read_one_block(buf, addr, rbsz, 0, NULL, 0);

        if (n < 16) {
            printf("  %5u    0x%08X  FAIL\n", blk, addr);
            fail_count++;
            /* Try with bitstream reload on failure */
            n = read_one_block(buf, addr, rbsz, 1, bs, asz);
            if (n < 16) { printf("  %5u    0x%08X  FAIL (retry)\n", blk, addr); continue; }
        }

        UCHAR *d = buf + 16;
        UINT dlen = n - 16;
        int all_ff=1, all_00=1, nz=0;
        for(UINT j=0;j<dlen&&j<2048;j++){if(d[j]!=0xFF)all_ff=0;if(d[j]!=0x00)all_00=0;if(d[j]!=0xFF&&d[j]!=0x00)nz++;}

        char *status = all_ff ? "BLANK" : all_00 ? "ZEROS" : "DATA!";
        if (!all_ff && !all_00) data_count++;
        else if (all_ff) blank_count++;
        else fail_count++;

        printf("  %5u    0x%08X  %-5s    ", blk, addr, status);
        for(int j=0;j<16&&j<(int)dlen;j++) printf("%02X ",d[j]);
        if(nz>0) printf(" (%d)",nz);
        printf("\n");

        if (fout && !all_ff && !all_00) {
            UINT hdr[2]={blk,addr}; fwrite(hdr,1,8,fout); fwrite(d,1,dlen,fout);
        }
    }

    printf("\n  Data: %u  Blank: %u  Fail: %u\n", data_count, blank_count, fail_count);
    if (fout) fclose(fout);
    free(bs); free(buf);
    printf("\nDone.\n");
    return 0;
}
