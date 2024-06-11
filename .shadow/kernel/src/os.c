#include <common.h>
sem_t empty, fill;
#define P kmt->sem_wait
#define V kmt->sem_signal
#define N 5
#define NPROD 4
#define NCONS 5
void Tproduce(void *arg) { while (1) { P(&empty); putch('('); V(&fill);  } }
void Tconsume(void *arg) { while (1) { P(&fill);  putch(')'); V(&empty); } }
static inline task_t *task_alloc() {
  return pmm->alloc(sizeof(task_t));
}

extern task_t *tasks[],*current_task;

static void os_init() {
    pmm->init();
    kmt->init();
    dev->init();
    printf("init finished\n");
    //kmt->create(task_alloc(),"_",NULL,NULL);
    kmt->sem_init(&empty, "empty", N);
    kmt->sem_init(&fill,  "fill",  0);
    for (int i = 0; i < NPROD; i++) {
        kmt->create(task_alloc(), "producer", Tproduce, NULL);
    }
    for (int i = 0; i < NCONS; i++) {
        kmt->create(task_alloc(), "consumer", Tconsume, NULL);
    }
}

static void os_run() {
    //printf("os_run\n");
    //iset(true);
    //printf("iset true\n");
    yield();
    //printf("over\n");
    for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
        putch(*s == '*' ? '0' + cpu_current() : *s);
    }
    while (1) ;
}

typedef struct hand{
    int seq,event;
    handler_t handler;
}hand;

hand table[1024],temp[1024];
int cnt=0;

int cmp1(hand a,hand b){
    return a.seq<b.seq;
}

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
    assert(ctx!=NULL);
    if(ev.event==EVENT_YIELD){
        if (!current_task) current_task = tasks[0];
        else current_task->context = ctx;
        do {
            current_task = current_task->next;
        } while (
            (current_task - tasks[0]) % cpu_count() != cpu_current()||
            current_task->status != RUNNING
        );
        current_task=current_task->next;
        current_task->status=RUNNING;
        return current_task->context;
    }
    else{
        if (!current_task) current_task = tasks[0];
        else current_task->context = ctx;
        Context *next = NULL;
        for (int i=1;i<=cnt;i++) {
            hand h=table[i];
            if (h.event == EVENT_NULL || h.event == ev.event) {
                Context *r = h.handler(ev, ctx);
                if (r) next = r;
            }
        }
        panic_on(!next, "return to NULL context");
        return next;
    }
}

static void os_on_irq(int seq, int event, handler_t handler){
    table[++cnt].event=event;
    table[cnt].handler=handler;
    table[cnt].seq=seq;
    merge(1,cnt);
}

MODULE_DEF(os) = {
    .init=os_init,
    .run=os_run,
    .trap=os_trap,
    .on_irq=os_on_irq,
};
