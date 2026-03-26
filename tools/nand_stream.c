/*
 * nand_stream.c - Try simplest streaming: just send READ_DATA per page
 * No END/BEGIN between reads. Just command + read, command + read.
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
    ULONG tmo = 15000;
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

int main(int argc, char **argv) {
    UCHAR m[512], r[64];
    const char *outfile = (argc > 1) ? argv[1] : "nand_dump.bin";
    printf("=== T76 NAND Stream Test ===\n\n");

    if (usb_open()) { printf("T76 not found\n"); return 1; }

    /* Device info */
    memset(m,0,8); snd(1,m,8); rcv(0x81,r,64);
    printf("FW: 0x%04X\n", r[4]|(r[5]<<8));

    /* Bitstream */
    FILE *f = fopen("algoT76\\T7_Nand_84.alg","rb");
    if(!f) f=fopen("algoT76/T7_Nand_84.alg","rb");
    if(!f){printf("No alg\n");usb_close();return 1;}
    fseek(f,0,SEEK_END);long fsz=ftell(f);fseek(f,0,SEEK_SET);
    UCHAR *raw=malloc(fsz);fread(raw,1,fsz,f);fclose(f);
    UINT asz=raw[4]|(raw[5]<<8)|(raw[6]<<16)|(raw[7]<<24);
    UCHAR *bs=calloc(1,asz);
    unsigned short *out=(unsigned short*)bs,*in=(unsigned short*)&raw[4096];
    unsigned short *ie=(unsigned short*)&raw[fsz],*oe=(unsigned short*)(bs+asz);
    while(in<ie&&out<oe){unsigned short v=*in++;if(v)*out++=v;else{if(in>=ie)break;
        unsigned short c=*in++;if(!c)break;if(out+c>oe)c=(unsigned short)(oe-out);
        memset(out,0,c*2);out+=c;}}
    free(raw);

    memset(m,0,8);m[0]=0x26;m[1]=0xAF;m[4]=0xEE;m[5]=0xDD;m[6]=0x55;m[7]=0xAA;
    snd(1,m,8);rcv(0x81,r,64);
    memset(m,0,8);m[0]=0x26;le16(&m[2],512);le32(&m[4],asz);
    snd(1,m,8);rcv(0x81,r,64);
    for(UINT i=0;i<asz;i+=504){UINT bsz=(i+504<=asz)?504:(asz-i);
        memset(m,0,512);m[0]=0x26;m[1]=0x01;le16(&m[2],(UINT)bsz);
        memcpy(&m[8],&bs[i],bsz);snd(1,m,512);}
    memset(m,0,8);m[0]=0x26;m[1]=0x02;snd(1,m,8);rcv(0x81,r,64);
    printf("Bitstream OK\n");
    Sleep(50);

    /* Single init */
    snd(0x01, nand_init, 64);
    snd(0x01, begin_trans, 128);
    memset(m,0,8);m[0]=0x39;snd(1,m,8);rcv(0x81,r,32);

    memset(m,0,8);m[0]=0x05;snd(1,m,8);rcv(0x81,r,32);
    printf("Chip: %02X %02X\n\n", r[2], r[3]);
    if(r[2]!=0x2C){printf("No chip\n");usb_close();return 1;}

    UINT rbsz = 8448;
    UINT total_blocks = 32768;
    UINT pages_per_read = 4;
    UCHAR *buf = calloc(1, rbsz + 16);

    FILE *fout = fopen(outfile, "wb");
    if(!fout){printf("Cannot create %s\n",outfile);usb_close();return 1;}

    printf("Reading %u blocks (just READ_DATA per block, no re-init)...\n\n", total_blocks);
    DWORD t0 = GetTickCount();
    UINT errors = 0;

    for (UINT blk = 0; blk < total_blocks; blk++) {
        UINT page = blk * pages_per_read;

        /* Just send READ_DATA with page number - nothing else between reads */
        memset(m,0,16); m[0]=0x10; le16(&m[2],rbsz); le32(&m[4],page);
        snd(0x01, m, 16);

        int n = rcv(0x82, buf, rbsz + 16);

        if (n < 16) {
            errors++;
            printf("\n  Block %u page %u: FAIL (err=%lu)\n", blk, page, GetLastError());
            if (errors > 3) {
                printf("  Streaming failed at block %u. This approach doesn't work.\n", blk);
                break;
            }
            memset(buf+16,0xFF,rbsz); n=rbsz+16;
        }

        fwrite(buf+16, 1, n-16, fout);

        if (blk % 256 == 0 || blk < 5 || blk == total_blocks-1) {
            DWORD el = GetTickCount() - t0;
            UINT pct = ((blk+1)*100)/total_blocks;
            UINT mb = (UINT)(((UINT64)(blk+1)*rbsz)/(1024*1024));
            float speed = el > 0 ? (float)mb*1000.0f/(float)el : 0;
            printf("\r  [");
            for(int i=0;i<40;i++) printf(i<(int)(pct*40/100)?"#":"-");
            printf("] %u%% %uMB %.1fMB/s  ", pct, mb, speed);
            fflush(stdout);
        }
    }

    printf("\n\n");
    fclose(fout);
    fout=fopen(outfile,"rb");
    if(fout){fseek(fout,0,SEEK_END);printf("Written: %ld bytes\n",ftell(fout));fclose(fout);}
    printf("Errors: %u\n", errors);

    memset(m,0,8);m[0]=0x04;snd(1,m,8);
    free(buf);usb_close();
    printf("Done.\n");
    return 0;
}
