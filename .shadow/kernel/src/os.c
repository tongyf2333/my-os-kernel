#include <common.h>
#define INT_MIN -2147483647
#define INT_MAX 2147483647
sem_t empty, fill;
#define P kmt->sem_wait
#define V kmt->sem_signal
#define N 5
#define NPROD 1
#define NCONS 1
void Tproduce(void *arg) { while (1) { P(&empty); putch('('); V(&fill);  } }
void Tconsume(void *arg) { while (1) { P(&fill);  putch(')'); V(&empty); } }
static inline task_t *task_alloc() {
  return pmm->alloc(sizeof(task_t));
}

extern task_t *tasks[],*current_task;

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

static Context *kmt_context_save(Event ev, Context *ctx){
    if (!current_task) current_task = tasks[0];
    else current_task->context = ctx;
    return current_task->context;
}
static Context *kmt_schedule(Event ev, Context *ctx){
    do {
        current_task = current_task->next;
    } while (
        current_task->cpu_id != cpu_current() ||
        current_task->status != RUNNING 
    );
    current_task=current_task->next;
    return current_task->context;
}
static Context *os_trap(Event ev, Context *ctx){
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

static void os_on_irq(int seq, int event, handler_t handler){
    table[++cnt].event=event;
    table[cnt].handler=handler;
    table[cnt].seq=seq;
    merge(1,cnt);
}

static void os_init() {
    pmm->init();
    kmt->init();
    os->on_irq(INT_MIN,EVENT_NULL,kmt_context_save);
    os->on_irq(INT_MAX,EVENT_NULL,kmt_schedule);
    //dev->init();
}

static void os_run() {
    for (const char *s = "inside CPU #*\n"; *s; s++) {
        putch(*s == '*' ? '0' + cpu_current() : *s);
    }
    kmt->sem_init(&empty, "empty", N);
    kmt->sem_init(&fill,  "fill",  0);
    for (int i = 0; i < NPROD; i++) {
        kmt->create(task_alloc(), "producer", Tproduce, NULL);
    }
    for (int i = 0; i < NCONS; i++) {
        kmt->create(task_alloc(), "consumer", Tconsume, NULL);
    }
    iset(true);
    yield();
    for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
        putch(*s == '*' ? '0' + cpu_current() : *s);
    }
    while (1) ;
}

MODULE_DEF(os) = {
    .init=os_init,
    .run=os_run,
    .trap=os_trap,
    .on_irq=os_on_irq,
};
