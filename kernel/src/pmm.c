#include <common.h>

#define LOCKED 1
#define UNLOCKED 0
#define MAGIC 12345678
#define MAGICC 87654321
typedef unsigned long long ull;

//find 2^i
size_t find(size_t x){if(!x) return 1;x|=(x>>1);x|=(x>>2);x|=(x>>4);x|=(x>>8);x|=(x>>16);return x+1;}
//implemented a simple spinlock
typedef struct Spinlock{int locked;}spinlock;
void spinlock_init(spinlock *lk){lk->locked=0;}
void spinlock_lock(spinlock *lk){
    retry:
    int status=atomic_xchg(&lk->locked,LOCKED);
    while(status!=UNLOCKED) goto retry;
}
void spinlock_unlock(spinlock *lk){atomic_xchg(&lk->locked,UNLOCKED);}
static spinlock lk;
//buddy system
typedef struct buddy_node{
    struct buddy_node *next;
}buddy_node;
buddy_node *head[21];
int lim,buddy_inited;
int getlog(int x){if(x==1) return 0;return getlog(x>>1)+1;}
int check(int x,int y,int line){
    if((x^y)==(1<<line)*4096) return 1;
    return 0;
}
int min(int x,int y){if(x<y) return x;return y;}
int start;

void buddy_insert(int line,int val){
    if(val%((1<<line)*4096)!=0){
        printf("%d %p\n",line,val);
    }
    assert(val%((1<<line)*4096)==0);
    buddy_node *tmp=(buddy_node*)(intptr_t)val;
    tmp->next=head[line];
    head[line]=tmp;
    assert((intptr_t)head[line]%((1<<line)*4096)==0);
}

void buddy_init(){
    start=16*1024*1024;
    while(start<(intptr_t)heap.start) start+=16*1024*1024;
    lim=getlog(16*1024*1024/4096);
    head[lim]=NULL;
    for(int i=start;i+16*1024*1024<=(intptr_t)heap.end;i+=16*1024*1024){
        buddy_insert(lim,i);
    }
    for(int i=0;i<lim;i++) head[i]=NULL;
}

int magic,sizz;

static void *pgalloc(int size){
    assert(size>=4096);
    if(!buddy_inited){
        buddy_inited=1;
        buddy_init();
    }
    int logg=getlog(size/4096),pos=logg,flag=0;
    for(int i=logg;i<=lim;i++){
        if(head[i]!=NULL){
            flag=1;
            pos=i;
            break;
        }
    }
    if(!flag) return NULL;
    void *ans=(void*)head[pos];
    if((intptr_t)ans%size!=0){
        printf("ans:%p,size:%d\n",ans,size);
    }
    assert((intptr_t)ans%size==0);
    if(ans==heap.start){
        magic=MAGIC;
        sizz=size;
    }
    else{
        *(int*)(ans-sizeof(int))=MAGIC;
        *(int*)(ans-2*sizeof(int))=size;
    }
    head[pos]=head[pos]->next;
    int i=logg,j=(intptr_t)ans+size,k=size;
    for(;i<pos&&j+k<=(intptr_t)heap.end;i++){
        buddy_insert(i,j);
        j+=k;
        k<<=1;
    }
    return ans;
}

static void pgfree(void *ptr){
    int size;
    if(ptr==heap.start) size=sizz;
    else size=*(int*)(ptr-2*sizeof(int));
    int logg=getlog(size/4096);
    int cur=(intptr_t)ptr,i=logg;
    for(;i<lim;i++){
        int flag=0;
        buddy_node *now=head[i];
        if(now!=NULL&&check((intptr_t)cur-start,(intptr_t)now-start,i)){
            cur=min(cur,(intptr_t)now);
            head[i]=now->next;
            flag=1;
        }
        else while(now!=NULL){
            assert(now!=now->next);
            if(now->next!=NULL&&check((intptr_t)cur-start,(intptr_t)now->next-start,i)){
                cur=min(cur,(intptr_t)now->next);
                now->next=now->next->next;
                flag=1;
                break;
            }
            else now=now->next;
        }
        if(!flag) break;
    }
    buddy_insert(i,cur);
}

//slab
typedef struct slab_node{
    struct slab_node *prev,*next;
}slab_node;

slab_node *header[100][120];

void insert(int id,int line,int val){
    assert(id<cpu_count());
    assert((val%(1<<line))==0);
    assert(val<(intptr_t)heap.end);
    slab_node *now=(slab_node*)(intptr_t)val;
    now->next=header[id][line];
    now->prev=NULL;
    if(header[id][line]!=NULL) header[id][line]->prev=now;
    header[id][line]=now;
    if(header[id][line]!=NULL) assert((intptr_t)header[id][line]<(intptr_t)heap.end);
    assert(header[id][line]->prev==NULL);
}
void delete(int id,int line){
    assert(id<cpu_count());
    assert(header[id][line]!=NULL);
    if(header[id][line]->next!=NULL) header[id][line]->next->prev=NULL;
    header[id][line]=header[id][line]->next;
    if(header[id][line]!=NULL){
        if(!((intptr_t)header[id][line]<(intptr_t)heap.end)){
            printf("line:%p,error:%p\n",line,header[id][line]);
            assert(0);
        }
    }
}

void slab_create(int id,int line,void *ptr){
    assert(id<cpu_count());
    int siz=1<<line;
    int cnt=(4096-5*sizeof(int))/siz;
    for(int i=0;i<cnt;i++){
        assert((intptr_t)ptr+i*siz<(intptr_t)heap.end);
        insert(id,line,(intptr_t)ptr+i*siz);
    }
    *(int*)((intptr_t)ptr+4096-3*sizeof(int))=siz;
    *(int*)((intptr_t)ptr+4096-4*sizeof(int))=cnt;
    *(int*)((intptr_t)ptr+4096-5*sizeof(int))=id;
    if(ptr==heap.start){
        magic=MAGICC;
    }
    else *(int*)((intptr_t)ptr-sizeof(int))=MAGICC;
}

void slab_insert(int id,int line,int val){
    assert(id<cpu_count());
    int siz=1<<line;
    insert(id,line,val);
    int *a=(int*)((intptr_t)val-((intptr_t)val&0xfff)+4096-4*sizeof(int));
    assert((*a)>=0&&(*a)<=(4096-5*sizeof(int))/siz);
    (*a)++;
    assert((*a)>=0&&(*a)<=(4096-5*sizeof(int))/siz);
    if((*a)==(4096-5*sizeof(int))/siz){
        for(int i=(val-(val&0xfff));i+siz<=(val-(val&0xfff))+4096-5*sizeof(int);i+=siz){
            slab_node *cur=(slab_node*)(intptr_t)i;
            if(cur->prev!=NULL){
                cur->prev->next=cur->next;
            }
            else if(cur->prev==NULL){
                header[id][line]=cur->next;
            }
            if(cur->next!=NULL) cur->next->prev=cur->prev;
        }
        spinlock_lock(&lk);
        pgfree((void*)(intptr_t)(val-(val&0xfff)));
        spinlock_unlock(&lk);
    }
}

//kalloc and kfree
static void *kalloc(size_t size) {
    if(find(size)>2048){
        spinlock_lock(&lk);
        void *ans=pgalloc(find(size+2*sizeof(int)));
        spinlock_unlock(&lk);
        return ans;
    }
    else{
        int id=cpu_current();
        int siz=find(size);
        if(siz<sizeof(slab_node)) siz=sizeof(slab_node);
        assert(siz<4096);
        int logg=getlog(siz);
        if(header[id][logg]==NULL){
            spinlock_lock(&lk);
            void *page=pgalloc(4096);
            spinlock_unlock(&lk);
            if(page==NULL){
                return NULL;
            }
            slab_create(id,logg,page);
            assert((intptr_t)page%siz==0);
        }
        void *ans=(void*)(intptr_t)header[id][logg];
        assert((intptr_t)ans%siz==0);
        int *a=(int*)((intptr_t)ans-((intptr_t)ans&0xfff)+4096-4*sizeof(int));
        assert((*a)>=0&&(*a)<=(4096-5*sizeof(int))/siz);
        (*a)--;
        assert((*a)>=0&&(*a)<=(4096-5*sizeof(int))/siz);

        delete(id,logg);
        return ans;
    }
    return NULL;
}

static void kfree(void *ptr) {
    spinlock_lock(&lk);
    if(ptr==heap.start){
        if(magic==MAGIC){
            pgfree(ptr);
            spinlock_unlock(&lk);
            return;
        }
    }
    else if(*(int*)(ptr-sizeof(int))==MAGIC){
        pgfree(ptr);
        spinlock_unlock(&lk);
        return;
    }
    spinlock_unlock(&lk);
    /*int page=((intptr_t)ptr-((intptr_t)ptr&0xfff));
    int siz=*(int*)((intptr_t)page+4096-3*sizeof(int));
    int id=*(int*)((intptr_t)page+4096-5*sizeof(int));
    assert(id<cpu_count());
    assert(siz!=0);
    int logg=getlog(siz);
    slab_insert(id,logg,(intptr_t)ptr);*/
}

static void pmm_init() {
    uintptr_t pmsize = (
        (uintptr_t)heap.end
        - (uintptr_t)heap.start
    );

    printf(
        "Got %d MiB heap: [%p, %p)\n",
        pmsize >> 20, heap.start, heap.end
    );
}

MODULE_DEF(pmm) = {
    .init  = pmm_init,
    .alloc = kalloc,
    .free  = kfree,
};
