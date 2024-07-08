#include <common.h>
#include <devices.h>
sem_t empty, fill;
#define P kmt->sem_wait
#define V kmt->sem_signal
#define N 1
#define NPROD 4
#define NCONS 4
typedef struct hand{
    int seq,event;
    handler_t handler;
}hand;
hand table[1024],temp[1024];
int cnt=0;
extern void solver();
//test semaphore
void Tproduce(void *arg) { while (1) { P(&empty);/*printf("[producer on %d]",cpu_current()+1);*/putch('('); V(&fill);  } }
void Tconsume(void *arg) { while (1) { P(&fill);/*printf("[consumer on %d]",cpu_current()+1);*/putch(')'); V(&empty); } }
//test spinlock
spinlock_t lkk;
void print1(){while(1){kmt->spin_lock(&lkk);putch('(');kmt->spin_unlock(&lkk);}}
void print2(){while(1){kmt->spin_lock(&lkk);putch(')');kmt->spin_unlock(&lkk);}}
static inline task_t *task_alloc() {return pmm->alloc(sizeof(task_t));}
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
static void tty_reader(void *arg) {
    struct device *tty = dev->lookup(arg);
    char cmd[128], resp[128], ps[16];
    snprintf(ps, 16, "(%s) $ ", arg);
    while (1) {
        tty->ops->write(tty, 0, ps, strlen(ps));
        int nread = tty->ops->read(tty, 0, cmd, sizeof(cmd) - 1);
        cmd[nread] = '\0';
        sprintf(resp, "tty reader task: got %d character(s).\n", strlen(cmd));
        tty->ops->write(tty, 0, resp, strlen(resp));
    }
}
/*
static void hard_test(){
    kmt->sem_init(&empty, "empty", N);
    kmt->sem_init(&fill,  "fill",  0);
    for (int i = 0; i < NPROD; i++) {
        kmt->create(task_alloc(), "producer", Tproduce, NULL);
    }
    for (int i = 0; i < NCONS; i++) {
        kmt->create(task_alloc(), "consumer", Tconsume, NULL);
    }
}*/
/*static void easy_test(){
    kmt->spin_init(&lkk,"lkk");
    kmt->create(task_alloc(),"print",print1,NULL);
    kmt->create(task_alloc(),"print",print2,NULL);
}*/
static void os_init() {
    pmm->init();
    kmt->init();
    dev->init();
    //easy_test();
    //hard_test();
    kmt->create(task_alloc(), "tty_reader", tty_reader, "tty1");
    kmt->create(task_alloc(), "tty_reader", tty_reader, "tty2");
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
