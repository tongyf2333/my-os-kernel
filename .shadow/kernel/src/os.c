#include <common.h>
#include<time.h>
#include<stdlib.h>
#define LOCKED 1
#define UNLOCKED 0
#define MAG 1145141919

typedef struct header{
    int len;
}header;

int* pp[60005];
int siz[60005];
int finish_cnt;

//implemented a simple spinlock
typedef struct Spinlock{int locked;}spinlock;

static void spinlock_lock(spinlock *lk){
    retry:
    int status=atomic_xchg(&lk->locked,LOCKED);
    while(status!=UNLOCKED) goto retry;
}

static void spinlock_unlock(spinlock *lk){atomic_xchg(&lk->locked,UNLOCKED);}

static void os_init() {
    pmm->init();
}

static spinlock lk;

void _debug(char *s){
    spinlock_lock(&lk);
    printf(s);
    spinlock_unlock(&lk);
}

const int dataset=10000;

int rnd(int maxn){
    return rand()%maxn+1;
}

static void os_run() {
    spinlock_lock(&lk);
    printf("start\n");
    spinlock_unlock(&lk);
    int cur=dataset*cpu_current();
    for(int i=0;i<dataset;i++){
        int size=rnd(4096);
        //spinlock_lock(&lk);
        pp[i+cur]=pmm->alloc(size);
        //spinlock_unlock(&lk);
        //assert(pp[i+dataset*cpu_current()]!=NULL);
        siz[i+cur]=size;
        if(pp[i+cur]!=NULL){
            for(int j=0;j<siz[i+cur]/sizeof(int);j++){
                if(pp[i+cur][j]==MAG) assert(0);
                pp[i+cur][j]=MAG;
            }
        }
    }

    spinlock_lock(&lk);
    for (const char *s = "allowcated CPU #*\n"; *s; s++) {
        putch(*s == '*' ? '0' + cpu_current() : *s);
    }
    spinlock_unlock(&lk);

    for(int i=0;i<dataset;i++){
        if(pp[i+cur]!=NULL){
            //printf("free\n");
            for(int j=0;j<siz[i+cur]/sizeof(int);j++){
                pp[i+cur][j]=0;
            }
            //spinlock_lock(&lk);
            pmm->free(pp[i+cur]);
            //spinlock_unlock(&lk);
        }
    }
    spinlock_lock(&lk);
    for (const char *s = "finished CPU #*\n"; *s; s++) {
        putch(*s == '*' ? '0' + cpu_current() : *s);
    }
    spinlock_unlock(&lk);

    for(int i=0;i<dataset;i++){
        int size=siz[i+cur];
        //spinlock_lock(&lk);
        pp[i+cur]=pmm->alloc(size);
        //spinlock_unlock(&lk);
        //assert(pp[i+dataset*cpu_current()]!=NULL);
        siz[i+cur]=size;
        if(pp[i+cur]!=NULL){
            for(int j=0;j<siz[i+cur]/sizeof(int);j++){
                if(pp[i+cur][j]==MAG) assert(0);
                pp[i+cur][j]=MAG;
            }
        }
    }

    spinlock_lock(&lk);
    for (const char *s = "allowcated CPU #*\n"; *s; s++) {
        putch(*s == '*' ? '0' + cpu_current() : *s);
    }
    spinlock_unlock(&lk);

    for(int i=0;i<dataset;i++){
        if(pp[i+cur]!=NULL){
            //printf("free\n");
            for(int j=0;j<siz[i+cur]/sizeof(int);j++){
                pp[i+cur][j]=0;
            }
            //spinlock_lock(&lk);
            pmm->free(pp[i+cur]);
            //spinlock_unlock(&lk);
        }
    }
    spinlock_lock(&lk);
    for (const char *s = "finished CPU #*\n"; *s; s++) {
        putch(*s == '*' ? '0' + cpu_current() : *s);
    }
    spinlock_unlock(&lk);
    spinlock_lock(&lk);
    finish_cnt++;
    printf("finish_cnt:%d\n",finish_cnt);
    spinlock_unlock(&lk);
    while (1) ;
}

MODULE_DEF(os) = {
    .init = os_init,
    .run  = os_run,
};
