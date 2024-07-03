#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "fat32.h"

struct fat32hdr *hdr;

wchar_t long_name[4096];

LFNEntry *names[256];

void *mmap_disk(const char *fname);
void dfs_scan(u32 clusId, int depth, int is_dir);

uint8_t calc_checksum(uint8_t *sfn){
    uint8_t sum = 0;
    for (int i = 11; i != 0; i--) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *sfn++;
    }
    return sum;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s fs-image\n", argv[0]);
        exit(1);
    }
    setbuf(stdout, NULL);
    assert(sizeof(struct fat32hdr) == 512);
    assert(sizeof(struct fat32dent) == 32);
    assert(sizeof(LFNEntry) == 32);

    hdr = mmap_disk(argv[1]);

    dfs_scan(hdr->BPB_RootClus, 0, 1);

    munmap(hdr, hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec);
}

void *mmap_disk(const char *fname) {
    int fd = open(fname, O_RDWR);

    if (fd < 0) {
        goto release;
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0) {
        goto release;
    }

    struct fat32hdr *hdr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (hdr == MAP_FAILED) {
        goto release;
    }

    close(fd);

    assert(hdr->Signature_word == 0xaa55); // this is an MBR
    assert(hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec == size);

    printf("%s: DOS/MBR boot sector, ", fname);
    printf("OEM-ID \"%s\", ", hdr->BS_OEMName);
    printf("sectors/cluster %d, ", hdr->BPB_SecPerClus);
    printf("sectors %d, ", hdr->BPB_TotSec32);
    printf("sectors %d, ", hdr->BPB_TotSec32);
    printf("sectors/FAT %d, ", hdr->BPB_FATSz32);
    printf("serial number 0x%x\n", hdr->BS_VolID);
    return hdr;

release:
    perror("map disk");
    if (fd > 0) {
        close(fd);
    }
    exit(1);
}

u32 next_cluster(int n) {
    // RTFM: Sec 4.1

    u32 off = hdr->BPB_RsvdSecCnt * hdr->BPB_BytsPerSec;
    u32 *fat = (u32 *)((u8 *)hdr + off);
    return fat[n];
}

void *cluster_to_sec(int n) {
    // RTFM: Sec 3.5 and 4 (TRICKY)
    // Don't copy code. Write your own.

    u32 DataSec = hdr->BPB_RsvdSecCnt + hdr->BPB_NumFATs * hdr->BPB_FATSz32;
    DataSec += (n - 2) * hdr->BPB_SecPerClus;
    return ((char *)hdr) + DataSec * hdr->BPB_BytsPerSec;
}

void get_filename(struct fat32dent *dent, char *buf) {
    // RTFM: Sec 6.1

    int len = 0;
    for (int i = 0; i < sizeof(dent->DIR_Name); i++) {
        if (dent->DIR_Name[i] != ' ') {
            if (i == 8)
                buf[len++] = '.';
            buf[len++] = dent->DIR_Name[i];
        }
    }
    buf[len] = '\0';
}

void dfs_scan(u32 clusId, int depth, int is_dir) {
    // RTFM: Sec 6

    for (; clusId < CLUS_INVALID; clusId = next_cluster(clusId)) {

        if (is_dir) {
            int ndents = hdr->BPB_BytsPerSec * hdr->BPB_SecPerClus / sizeof(struct fat32dent);

            for (int d = 0; d < ndents; d++) {
                struct fat32dent *dent = (struct fat32dent *)cluster_to_sec(clusId) + d;
                LFNEntry *dt=(LFNEntry*)cluster_to_sec(clusId)+d;
                if (dent->DIR_Name[0] == 0x00 ||
                    dent->DIR_Name[0] == 0xe5 ||
                    dent->DIR_Attr & ATTR_HIDDEN)
                    continue;

                char fname[32];
                get_filename(dent, fname);

                for (int i = 0; i < 4 * depth; i++)
                    putchar(' ');

                if(dt->attr!=0xf){
                    int cnt=0;
                    uint8_t checksum=calc_checksum(dent->DIR_Name);
                    for(int i=0;i<256;i++) names[i]=NULL;
                    for(int i=d-1;i>=0;i--){
                        LFNEntry *dtt=(LFNEntry*)cluster_to_sec(clusId)+i;
                        if(checksum!=dtt->checksum) continue;
                        if(dtt->attr==0xf){
                            names[dtt->order]=dtt;
                        }
                        if(dtt->name3[1]==0xffff) break;
                    }
                    for(int i=0;i<256;i++){
                        if(names[i]!=NULL){
                            for(int j=0;j<5;j++) long_name[cnt++]=names[i]->name1[j];
                            for(int j=0;j<6;j++) long_name[cnt++]=names[i]->name2[j];
                            for(int j=0;j<2;j++) long_name[cnt++]=names[i]->name3[j];
                        }
                    }
                    long_name[cnt++]='\0';
                    printf("long name:%ls\n",long_name);
                }

                printf("[%-12s] %6.1lf KiB    ", fname, dent->DIR_FileSize / 1024.0);

                u32 dataClus = dent->DIR_FstClusLO | (dent->DIR_FstClusHI << 16);
                if (dent->DIR_Attr & ATTR_DIRECTORY) {
                    
                    printf("\n");
                    if (dent->DIR_Name[0] != '.') {
                        dfs_scan(dataClus, depth + 1, 1);
                    }
                } else {
                    dfs_scan(dataClus, depth + 1, 0);
                    printf("\n");
                }
            }
        } else {
            printf("#%d ", clusId);
        }
    }
}