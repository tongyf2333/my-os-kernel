#include <common.h>
#include <devices.h>
sem_t empty, fill;
#define P kmt->sem_wait
#define V kmt->sem_signal
#define N 2
#define NPROD 5
#define NCONS 5
typedef struct hand{
    int seq,event;
    handler_t handler;
}hand;
hand table[1024],temp[1024];
int cnt=0;
extern void solver();
extern struct task *current_task[];
//test semaphore
static inline task_t *task_alloc() {return pmm->alloc(sizeof(task_t));}
void Tproduce(void *arg) { while (1) { P(&empty);putch('('); V(&fill);  } }
void Tconsume(void *arg) { while (1) { P(&fill);putch(')'); V(&empty); } }
int cmp1(hand a,hand b){return a.seq<b.seq;}
void merge(int l,int r){
	if(l==r) return;
	int mid=(l+r)/2;
	merge(l,mid);
	merge(mid+1,r);
	int hd=l,tl=mid+1,now=l;
	while(hd<=mid&&tl<=r){
		if(cmp1(table[hd],table[tl])) temp[now++]=table[hd++];
		else temp[now++]=table[tl++];
	}
	while(hd<=mid) temp[now++]=table[hd++];
	while(tl<=r) temp[now++]=table[tl++];
	for(int i=l;i<=r;i++) table[i]=temp[i];
}
static Context *os_trap(Event ev, Context *ctx){
    Context *next = NULL;
    for (int i=1;i<=cnt;i++) {
        hand h=table[i];
        if (h.event == EVENT_NULL || h.event == ev.event) {
            Context *r = h.handler(ev, ctx);
            panic_on(r && next, "returning multiple contexts");
            if (r!=NULL) next = r;
        }
    }
    if(!next) printf("event:%d\n",ev.event+1);
    panic_on(!next, "return to NULL context");
    return next;
}
static void os_on_irq(int seq, int event, handler_t handler){
    table[++cnt].event=event;
    table[cnt].handler=handler;
    table[cnt].seq=seq;
    merge(1,cnt);
}

static void hard_test(){
    kmt->sem_init(&empty, "empty", N);
    kmt->sem_init(&fill,  "fill",  0);
    for (int i = 0; i < NPROD; i++) {
        kmt->create(task_alloc(), "producer", Tproduce, NULL);
    }
    for (int i = 0; i < NCONS; i++) {
        kmt->create(task_alloc(), "consumer", Tconsume, NULL);
    }
}

static void os_init() {
    pmm->init();
    kmt->init();
    //dev->init();
    hard_test();
}
static void os_run() {
    solver();
}
MODULE_DEF(os) = {
    .init=os_init,
    .run=os_run,
    .trap=os_trap,
    .on_irq=os_on_irq,
};
