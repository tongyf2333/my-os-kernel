#include <common.h>
#define INT_MIN -2147483647
#define INT_MAX 2147483647
sem_t empty, fill;
#define P kmt->sem_wait
#define V kmt->sem_signal
#define N 5
#define NPROD 2
#define NCONS 2
void Tproduce(void *arg) { while (1) { P(&empty); putch('('); V(&fill);  } }
void Tconsume(void *arg) { while (1) { P(&fill);  putch(')'); V(&empty); } }
void solver(void *arg){while(1);}
static inline task_t *task_alloc() {
  return pmm->alloc(sizeof(task_t));
}

extern task_t *tasks[],*current_task[];
extern int task_count;

typedef struct hand{
    int seq,event;
    handler_t handler;
}hand;

hand table[1024],temp[1024];
int cnt=0,sum=0;

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
    if (current_task[cpu_current()]==NULL) current_task[cpu_current()] = tasks[0];
    else current_task[cpu_current()]->context = ctx;
    //return current_task[cpu_current()]->context;
    return NULL;
}
static Context *kmt_schedule(Event ev, Context *ctx){//bug here
    do {
        current_task[cpu_current()] = current_task[cpu_current()]->next;
        //printf("scheduling %d\n",current_task[cpu_current()]->id+1);
    } while (
        current_task[cpu_current()]->status != RUNNING ||
        ((current_task[cpu_current()]->cpu_id!=-1)&&(current_task[cpu_current()]->cpu_id!=cpu_current()))
    );
    current_task[cpu_current()]->cpu_id=cpu_current();
    //printf("%d",current_task[cpu_current()]->id+1);
    return current_task[cpu_current()]->context;
}
static Context *os_trap(Event ev, Context *ctx){
    //printf("VVV\n");
    //printf("%d\n",ev.event+1);
    Context *next = NULL;
    for (int i=1;i<=cnt;i++) {
        hand h=table[i];
        //if(ev.event==EVENT_IRQ_IODEV) printf("XXX\n");
        if (h.event == EVENT_NULL || h.event == ev.event) {
            //printf("%d\n",h.event+1);
            Context *r = h.handler(ev, ctx);
            panic_on(r && next, "returning multiple contexts");
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

static void hard_test(){
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
    kmt->create(task_alloc(),"irq",solver,NULL);
    os->on_irq(INT_MIN,EVENT_NULL,kmt_context_save);
    os->on_irq(INT_MAX,EVENT_NULL,kmt_schedule);
    //dev->init();
    kmt->sem_init(&empty, "empty", N);
    kmt->sem_init(&fill,  "fill",  0);
    hard_test();
    //while(1);
}

static void os_run() {
    //hard_test();
    iset(true);
    yield();
    assert(0);
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
