/*
 * nand_read_v1.c - Exact reproduction of the successful read
 * Tests 1-3 (READ_CODE) may be needed as "warmup" before
 * Test 4 (READ_DATA) works. Stops after first success.
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
        SetupDiDestroyDeviceInfoList(di); return -1;
    }
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
    ULONG t; return WinUsb_WritePipe(hUSB, ep, d, len, &t, NULL) ? (int)t : -1;
}
static int rcv(UCHAR ep, UCHAR *d, int len) {
    ULONG t=0; if (!WinUsb_ReadPipe(hUSB, ep, d, len, &t, NULL)) return -1;
    return (int)t;
}
static void le16(UCHAR *b, UINT v) { b[0]=v&0xFF; b[1]=(v>>8)&0xFF; }
static void le32(UCHAR *b, UINT v) { b[0]=v&0xFF; b[1]=(v>>8)&0xFF; b[2]=(v>>16)&0xFF; b[3]=(v>>24)&0xFF; }

static void begin_trans(UCHAR *m, UCHAR *r) {
    memset(m,0,64);
    m[0]=3;m[1]=0x2D;m[4]=6;m[6]=0xA0;m[7]=0x6F;
    m[10]=0;m[11]=8;m[12]=0x40;
    m[16]=0;m[17]=0;m[18]=0x80;m[19]=0x10;
    m[20]=8;m[21]=6;m[40]=8;m[44]=0;m[45]=0x21;m[56]=0x78;m[63]=0x84;
    snd(1,m,64);
    memset(m,0,8); m[0]=0x39; snd(1,m,8); rcv(0x81,r,64);
}
static void end_trans(UCHAR *m) {
    memset(m,0,8); m[0]=0x04; snd(1,m,8);
}

int main(int argc, char **argv) {
    UCHAR m[512], r[64];
    const char *outfile = (argc > 1) ? argv[1] : NULL;

    printf("=== T76 NAND Read (v1 reproduction) ===\n\n");

    if (usb_open()) { printf("T76 not found\n"); return 1; }

    /* Device info */
    memset(m,0,8); snd(1,m,8); rcv(0x81,r,64);
    printf("FW: 0x%04X  Code: %.8s\n", r[4]|(r[5]<<8), &r[24]);

    /* Load + upload bitstream */
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
    if(r[1]){printf("BS BEGIN fail\n");free(bs);usb_close();return 1;}
    for(UINT i=0;i<asz;i+=504){UINT bsz=(i+504<=asz)?504:(asz-i);memset(m,0,512);m[0]=0x26;m[1]=0x01;le16(&m[2],(UINT)bsz);memcpy(&m[8],&bs[i],bsz);snd(1,m,512);}
    memset(m,0,8);m[0]=0x26;m[1]=0x02;snd(1,m,8);rcv(0x81,r,64);
    free(bs);
    if(r[1]){printf("BS FAIL\n");usb_close();return 1;}
    printf("Bitstream OK\n");
    Sleep(50);

    /* BEGIN_TRANS */
    begin_trans(m, r);
    printf("OVC: %02X\n", r[0]);

    /* READID */
    memset(m,0,8); m[0]=0x05; snd(1,m,8); rcv(0x81,r,64);
    printf("Chip ID: %02X %02X %02X %02X %02X\n", r[3],r[4],r[5],r[6],r[7]);

    UCHAR *buf = calloc(1, 16384);
    ULONG transferred;
    BOOL ok;

    /*
     * Exact reproduction of the successful sequence:
     * Test 1: READ_CODE size=8448 blocks=1 (will fail - but may init state)
     * Test 2: READ_CODE size=2112 blocks=1 (will fail)
     * Test 3: READ_CODE size=2048 blocks=1 (will fail)
     * Test 4: READ_DATA size=8448 (SUCCESS!)
     */

    /* Test 1 */
    printf("\n[1] READ_CODE size=8448 blocks=1\n");
    end_trans(m); begin_trans(m, r);
    memset(m,0,16); m[0]=0x0D; le16(&m[2],8448); le32(&m[8],1);
    snd(1,m,16);
    transferred = 0;
    ok = WinUsb_ReadPipe(hUSB, 0x82, buf, 8448, &transferred, NULL);
    printf("  EP82: %s t=%lu err=%lu\n", ok?"OK":"FAIL", transferred, ok?0:GetLastError());
    if (transferred > 0) { printf("  GOT DATA in test 1!\n"); goto success; }

    /* Test 2 */
    printf("[2] READ_CODE size=2112 blocks=1\n");
    end_trans(m); begin_trans(m, r);
    memset(m,0,16); m[0]=0x0D; le16(&m[2],2112); le32(&m[8],1);
    snd(1,m,16);
    transferred = 0;
    ok = WinUsb_ReadPipe(hUSB, 0x82, buf, 2112, &transferred, NULL);
    printf("  EP82: %s t=%lu err=%lu\n", ok?"OK":"FAIL", transferred, ok?0:GetLastError());
    if (transferred > 0) { printf("  GOT DATA in test 2!\n"); goto success; }

    /* Test 3 */
    printf("[3] READ_CODE size=2048 blocks=1\n");
    end_trans(m); begin_trans(m, r);
    memset(m,0,16); m[0]=0x0D; le16(&m[2],2048); le32(&m[8],1);
    snd(1,m,16);
    transferred = 0;
    ok = WinUsb_ReadPipe(hUSB, 0x82, buf, 2048, &transferred, NULL);
    printf("  EP82: %s t=%lu err=%lu\n", ok?"OK":"FAIL", transferred, ok?0:GetLastError());
    if (transferred > 0) { printf("  GOT DATA in test 3!\n"); goto success; }

    /* Test 4 - THE ONE THAT WORKED */
    printf("[4] READ_DATA (0x10) size=8448 *** THE MONEY SHOT ***\n");
    end_trans(m); begin_trans(m, r);
    memset(m,0,16); m[0]=0x10; le16(&m[2],8448);
    snd(1,m,16);
    transferred = 0;
    ok = WinUsb_ReadPipe(hUSB, 0x82, buf, 8448+16, &transferred, NULL);
    printf("  EP82: %s t=%lu err=%lu\n", ok?"OK":"FAIL", transferred, ok?0:GetLastError());
    if (transferred > 0) goto success;

    printf("\nAll 4 tests failed.\n");
    goto done;

success:
    printf("\n*** SUCCESS! Got %lu bytes ***\n", transferred);
    printf("First 128 bytes:\n");
    for (ULONG i = 0; i < 128 && i < transferred; i++) {
        printf("%02X ", buf[i]);
        if ((i+1)%16==0) printf("\n");
    }
    printf("\n");
    {
        int nz=0; for(ULONG i=0;i<transferred;i++) if(buf[i]!=0xFF&&buf[i]!=0x00) nz++;
        printf("Non-trivial bytes: %d/%lu\n", nz, transferred);
    }

    /* If outfile specified, do full dump */
    if (outfile && transferred > 16) {
        printf("\n=== Full dump to %s ===\n", outfile);
        UINT rbsz = 8448;
        UINT blocks = 276824064 / rbsz;
        UINT pktsz = rbsz + 16;

        /*
         * Strategy: replay the EXACT winning sequence from scratch.
         * Tests 1-3 (READ_CODE fails) seem to prime the state.
         * Test 4 (READ_DATA) works.
         * Then DON'T send new commands -- just keep reading EP 0x82
         * for all remaining blocks (streaming mode).
         */
        end_trans(m);
        begin_trans(m, r);

        /* Replay warmup: tests 1-3 */
        printf("Warming up (tests 1-3)...\n");
        for (int warmup = 0; warmup < 3; warmup++) {
            end_trans(m); begin_trans(m, r);
            UINT wsz = (warmup == 0) ? 8448 : (warmup == 1) ? 2112 : 2048;
            memset(m,0,16); m[0]=0x0D; le16(&m[2],wsz); le32(&m[8],1);
            snd(1,m,16);
            ULONG dummy = 0;
            WinUsb_ReadPipe(hUSB, 0x82, buf, wsz, &dummy, NULL); /* will timeout, that's ok */
        }

        /* Test 4: READ_DATA - the one that works */
        end_trans(m); begin_trans(m, r);
        memset(m,0,16); m[0]=0x10; le16(&m[2],rbsz);
        snd(1,m,16);

        FILE *fout = fopen(outfile, "wb");
        if (!fout) { printf("Cannot create file\n"); goto done; }

        DWORD t0 = GetTickCount();
        UINT errors = 0;

        /* Read ALL blocks by just keep reading EP 0x82 (streaming) */
        printf("Reading %u blocks (streaming from EP 0x82)...\n\n", blocks);
        for (UINT blk = 0; blk < blocks; blk++) {
            int n = rcv(0x82, buf, pktsz);

            if (n < 16) {
                DWORD err = GetLastError();
                errors++;
                printf("\n  Block %u: FAIL (got %d bytes, WinUSB err=%lu)\n",
                       blk, n, err);
                if (err == 121) printf("  -> Timeout: device stopped sending data\n");
                if (errors > 5) { printf("  Stopping after %u errors.\n", errors); break; }
                memset(buf,0xFF,rbsz); fwrite(buf,1,rbsz,fout);
                continue;
            }

            /* Strip 16-byte header */
            UINT dlen = (UINT)n - 16;
            if (dlen > rbsz) dlen = rbsz;
            fwrite(buf+16, 1, dlen, fout);

            /* Verbose: show first few blocks */
            if (blk < 3) {
                printf("  Block %u: got %d bytes (data=%u)\n", blk, n, dlen);
                printf("    Header: ");
                for (int i=0; i<16; i++) printf("%02X ", buf[i]);
                printf("\n    Data[0..15]: ");
                for (int i=16; i<32 && i<n; i++) printf("%02X ", buf[i]);
                printf("\n");
            }

            if (blk%64==0 || blk==blocks-1) {
                DWORD el=(GetTickCount()-t0)/1000;
                UINT mb=(UINT)(((UINT64)(blk+1)*rbsz)/(1024*1024));
                printf("\r  ["); for(int i=0;i<40;i++) printf(i<(int)(((blk+1)*40)/blocks)?"#":"-");
                printf("] %u%% %uMB %us  ", ((blk+1)*100)/blocks, mb, (UINT)el);
                fflush(stdout);
            }
        }
        printf("\n\n");
        fclose(fout);

        fout = fopen(outfile,"rb");
        if(fout){fseek(fout,0,SEEK_END);printf("Written: %ld bytes\n",ftell(fout));fclose(fout);}
        printf("Errors: %u\n", errors);
    }

done:
    end_trans(m);
    free(buf);
    usb_close();
    printf("\nDone.\n");
    return 0;
}
