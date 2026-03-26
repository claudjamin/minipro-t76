/*
 * nand_read_v2.c - NAND reader using exact Xgpro protocol from USB capture
 *
 * Key discovery: Xgpro sends NAND_INIT (0x02) + 128-byte BEGIN_TRANS
 * before READID/READ operations. Without NAND_INIT, the FPGA doesn't
 * configure the NAND interface properly.
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
    if (hDev != INVALID_HANDLE_VALUE) CloseHandle(hDev);
}
static int snd(UCHAR ep, UCHAR *d, int len) {
    ULONG t; return WinUsb_WritePipe(hUSB, ep, d, len, &t, NULL) ? (int)t : -1; }
static int rcv(UCHAR ep, UCHAR *d, int len) {
    ULONG t=0; if (!WinUsb_ReadPipe(hUSB, ep, d, len, &t, NULL)) return -1;
    return (int)t; }
static void le16(UCHAR *b, UINT v) { b[0]=v&0xFF; b[1]=(v>>8)&0xFF; }
static void le32(UCHAR *b, UINT v) { b[0]=v&0xFF; b[1]=(v>>8)&0xFF; b[2]=(v>>16)&0xFF; b[3]=(v>>24)&0xFF; }

int main(int argc, char **argv) {
    UCHAR m[512], r[64];
    const char *outfile = (argc > 1) ? argv[1] : "nand_dump.bin";

    printf("=== T76 NAND Reader v2 (from USB capture) ===\n\n");

    if (usb_open()) { printf("T76 not found\n"); return 1; }

    /* Device info */
    memset(m,0,8); snd(1,m,8); rcv(0x81,r,64);
    printf("FW: 0x%04X  Code: %.8s\n", r[4]|(r[5]<<8), &r[24]);

    /* Load + upload bitstream (same as before - this part works) */
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

    /* FPGA Reset */
    memset(m,0,8);m[0]=0x26;m[1]=0xAF;m[4]=0xEE;m[5]=0xDD;m[6]=0x55;m[7]=0xAA;
    snd(1,m,8); rcv(0x81,r,64);

    /* Upload bitstream */
    printf("Uploading bitstream...\n");
    memset(m,0,8);m[0]=0x26;le16(&m[2],512);le32(&m[4],asz);
    snd(1,m,8); rcv(0x81,r,64);
    if(r[1]){printf("BS fail\n");free(bs);usb_close();return 1;}
    for(UINT i=0;i<asz;i+=504){UINT bsz=(i+504<=asz)?504:(asz-i);memset(m,0,512);m[0]=0x26;m[1]=0x01;le16(&m[2],(UINT)bsz);memcpy(&m[8],&bs[i],bsz);snd(1,m,512);}
    memset(m,0,8);m[0]=0x26;m[1]=0x02;snd(1,m,8);rcv(0x81,r,64);
    free(bs);
    if(r[1]){printf("BS upload fail\n");usb_close();return 1;}
    printf("Bitstream OK\n");
    Sleep(50);

    /*
     * === KEY DISCOVERY FROM USB CAPTURE ===
     * Xgpro sends NAND_INIT (0x02) before BEGIN_TRANS!
     * And BEGIN_TRANS is 128 bytes, not 64!
     * These are the EXACT bytes captured from Xgpro.
     */

    /* NAND_INIT (command 0x02) - 64 bytes */
    printf("Sending NAND_INIT (0x02)...\n");
    UCHAR nand_init[64] = {
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x08, 0x00, 0x08, 0x40, 0x00,
        0x01, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x3B, 0x4F, 0x15, 0x27,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    snd(0x01, nand_init, 64);

    /* BEGIN_TRANS - 128 bytes (not 64!) */
    printf("Sending BEGIN_TRANS (128 bytes)...\n");
    UCHAR begin_trans[128] = {
        0x03, 0x2D, 0x00, 0x00, 0x06, 0x00, 0xA0, 0x6F,
        0x00, 0x00, 0x00, 0x08, 0x40, 0x00, 0x20, 0x00,
        0x00, 0x10, 0x02, 0x00, 0x00, 0x06, 0x00, 0x00,
        0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x78, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x84,
        /* Second 64 bytes */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x2F, 0x27, 0x09, 0x0B, 0x00, 0x03, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    snd(0x01, begin_trans, 128);

    /* OVC STATUS */
    memset(m,0,8); m[0]=0x39; snd(1,m,8); rcv(0x81,r,32);
    printf("OVC: %02X\n", r[0]);

    /* READID */
    memset(m,0,8); m[0]=0x05; snd(1,m,8); rcv(0x81,r,32);
    printf("Chip ID: %02X %02X %02X %02X %02X\n", r[2],r[3],r[4],r[5],r[6]);

    if (r[2] == 0x2C) {
        printf("  Micron NAND detected! ID match!\n");
    } else if (r[2] == 0x00) {
        printf("  No chip response. Check adapter.\n");
        usb_close(); return 1;
    }

    /* Now try READ_DATA (0x10) - the command that worked before */
    printf("\nAttempting READ_DATA...\n");
    UINT rbsz = 8448;
    memset(m,0,16); m[0]=0x10; le16(&m[2],rbsz);
    snd(0x01, m, 16);

    UCHAR *buf = calloc(1, rbsz + 16);
    int n = rcv(0x82, buf, rbsz + 16);
    printf("EP 0x82: got %d bytes\n", n);

    if (n > 16) {
        printf("SUCCESS! First 64 data bytes:\n");
        for (int i = 16; i < 80 && i < n; i++) {
            printf("%02X ", buf[i]);
            if ((i-15) % 16 == 0) printf("\n");
        }
        printf("\n");

        int nz=0; for(int i=16;i<n;i++) if(buf[i]!=0xFF&&buf[i]!=0x00) nz++;
        printf("Non-trivial bytes: %d/%d\n\n", nz, n-16);

        /* Full dump - one block per transaction cycle */
        printf("=== Full NAND dump to %s ===\n", outfile);
        UINT total = 276824064;
        UINT blocks = total / rbsz;

        FILE *fout = fopen(outfile, "wb");
        if (!fout) { printf("Cannot create file\n"); goto done; }

        /* Write first block we already read (skip 16-byte header) */
        fwrite(buf+16, 1, n-16, fout);

        DWORD t0 = GetTickCount();
        UINT errors = 0;

        for (UINT blk = 1; blk < blocks; blk++) {
            /* End previous transaction */
            memset(m,0,4); m[0]=0x04; snd(1,m,4);

            /* NAND_INIT + BEGIN_TRANS for each block */
            snd(0x01, nand_init, 64);
            snd(0x01, begin_trans, 128);
            memset(m,0,8); m[0]=0x39; snd(1,m,8); rcv(0x81,r,32);

            /* READ_DATA with address for this block */
            UINT addr = blk * rbsz;
            memset(m,0,16); m[0]=0x10; le16(&m[2],rbsz); le32(&m[4],addr);
            snd(0x01, m, 16);

            n = rcv(0x82, buf, rbsz + 16);
            if (n < 16) {
                errors++;
                printf("\n  Block %u: FAIL (err=%lu)\n", blk, GetLastError());
                if (errors > 10) { printf("Stopping.\n"); break; }
                memset(buf,0xFF,rbsz); fwrite(buf,1,rbsz,fout);
                continue;
            }
            fwrite(buf+16, 1, n-16, fout);

            if (blk%32==0 || blk==blocks-1) {
                DWORD el=(GetTickCount()-t0)/1000;
                UINT mb=(UINT)(((UINT64)(blk+1)*rbsz)/(1024*1024));
                printf("\r  ["); for(int i=0;i<40;i++) printf(i<(int)(((blk+1)*40)/blocks)?"#":"-");
                printf("] %u%% %uMB %us  ", ((blk+1)*100)/blocks, mb, (UINT)el);
                fflush(stdout);
            }
        }
        printf("\n\n");
        fclose(fout);
        fout=fopen(outfile,"rb");
        if(fout){fseek(fout,0,SEEK_END);printf("Written: %ld bytes\n",ftell(fout));fclose(fout);}
        printf("Errors: %u\n", errors);
    } else {
        printf("FAILED - no data.\n");
    }

done:
    memset(m,0,8); m[0]=0x04; snd(1,m,8);
    free(buf);
    usb_close();
    printf("\nDone.\n");
    return 0;
}
