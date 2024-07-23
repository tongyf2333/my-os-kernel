#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h>
#include "fat32.h"

struct fat32hdr *hdr;
wchar_t long_name[4096];
LFNEntry *names[256];
int clussiz;
char buffer[16*1024*1024],buf[128];

int abs(int x){
    if(x<0) return -x;
    return x;
}

int min(int x,int y){
    if(x<y) return x;
    return y;
}

uint8_t calc_checksum(uint8_t *sfn){
    uint8_t sum = 0;
    for (int i = 11; i != 0; i--) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *sfn++;
    }
    return sum;
}

void *cluster_to_sec(int n) {
    u32 DataSec = hdr->BPB_RsvdSecCnt + hdr->BPB_NumFATs * hdr->BPB_FATSz32;
    DataSec += (n - 2) * hdr->BPB_SecPerClus;
    return ((char *)hdr) + DataSec * hdr->BPB_BytsPerSec;
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

    return hdr;

release:
    perror("map disk");
    if (fd > 0) {
        close(fd);
    }
    exit(1);
}

long l1_loss(char *a,char *b,int len){
    long ans=0;
    for(int i=0;i<len;i++){
        ans+=abs((*a)-(*b));
        a++,b++;
    }
    return ans;
}

char pool[16*1024*1024+10];
int vis[10000005];

int getsha(char *begin,int clusid,int size){
    assert(begin[0]==0x42&&begin[1]==0x4d);
    uint32_t first_data_sector = hdr->BPB_RsvdSecCnt + (hdr->BPB_NumFATs * hdr->BPB_FATSz32);
    uint32_t total_clusters = (hdr->BPB_TotSec32 - first_data_sector) / hdr->BPB_SecPerClus;
    char *cur=begin;int cid=clusid;
    int width=*(int*)(begin+18);
    int len=3*width,loaded=0;
    if(len%4!=0) len=len+(4-(len%4));
    memset(pool,0,sizeof(pool));
    memset(vis,0,sizeof(vis));
    vis[clusid]=1;
    long before=-1;
    for(int i=0,j=0;i<size;i+=clussiz,j++){
        char *end=cur+clussiz;
        if(!i) loaded+=(clussiz-54);
        else loaded+=clussiz;
        char *lastline=end-(loaded%len)-len;
        memcpy(pool+i,cur,clussiz);
        int part1=loaded%len,part2=len-loaded%len;
        long vall=l1_loss(lastline,lastline+len,len);
        long minn=vall;
        int minclus=cid+1;
        if(vall>50*before&&before!=-1){
            for(int kk=2;kk<total_clusters;kk++){
                if(vis[kk]==1) continue;
                char *head=(char*)cluster_to_sec(kk);
                long sum=l1_loss(lastline,lastline+len,part1)+l1_loss(lastline+part1,head,part2);
                if(sum<minn) minn=sum,minclus=kk;
            }
        }
        vis[minclus]=1;
        before=minn;
        cur=(char*)cluster_to_sec(minclus);cid=minclus;
        if(minclus>=total_clusters) break;
    }
    char *file_path = "/tmp/a.bin";
    int written=0;
    FILE *file = fopen(file_path, "wb");
    fwrite(pool,sizeof(char),size,file);
    fclose(file);
    FILE *fp=popen("sha1sum /tmp/a.bin","r");
    fscanf(fp,"%40s",buf);
    pclose(fp);
    printf("%s ",buf);
    return 0;
}

void solve(){
    uint32_t first_data_sector = hdr->BPB_RsvdSecCnt + (hdr->BPB_NumFATs * hdr->BPB_FATSz32);
    uint32_t total_clusters = (hdr->BPB_TotSec32 - first_data_sector) / hdr->BPB_SecPerClus;
    clussiz=hdr->BPB_BytsPerSec*hdr->BPB_SecPerClus;
    for (int clusId=2; clusId < total_clusters; clusId ++) {
        int bmpcnt=0;
        uint8_t *head=(uint8_t*)cluster_to_sec(clusId),*tl=(uint8_t*)cluster_to_sec(clusId+1);
        for(uint8_t *now=head;now<tl;now++){
            if(now+2>=tl) break;
            if(*now=='B'&&*(now+1)=='M'&&*(now+2)=='P') bmpcnt++;
        }
        if(bmpcnt>=9){
            int ndents = hdr->BPB_BytsPerSec * hdr->BPB_SecPerClus / sizeof(struct fat32dent);
            for (int d = 0; d < ndents; d++) {
                struct fat32dent *dent = (struct fat32dent *)cluster_to_sec(clusId) + d;
                if (dent->DIR_Name[0] == 0x00 ||
                    dent->DIR_Name[0] == 0xe5 ||
                    dent->DIR_Attr & ATTR_HIDDEN)
                    continue;

                if(dent->DIR_Attr==0x20){
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
                    u32 dataClus = dent->DIR_FstClusLO | (dent->DIR_FstClusHI << 16);
                    char *pointer=cluster_to_sec(dataClus);
                    getsha(pointer,dataClus,dent->DIR_FileSize);
                    printf(" %ls\n",long_name);
                }
            }
        }
    }
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
    solve();
    munmap(hdr, hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec);
}