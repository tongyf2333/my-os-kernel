#include <common.h>

extern task_t *tasks[],*current_task;

static void os_init() {
    pmm->init();
    kmt->init();
    dev->init();
}

static void os_run() {
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
        merge(1,cnt);
        Context *next = NULL;
        printf("event:%d\n",ev.event);
        for (int i=1;i<=cnt;i++) {
            hand h=table[i];
            if (h.event == EVENT_NULL || h.event == ev.event) {
                Context *r = h.handler(ev, ctx);
                //panic_on(r && next, "return to multiple contexts");
                if (r) next = r;
            }
        }
        panic_on(!next, "return to NULL context");
        //panic_on(sane_context(next), "return to invalid context");
        return next;
    }
}

static void os_on_irq(int seq, int event, handler_t handler){
    table[++cnt].event=event;
    table[cnt].handler=handler;
    table[cnt].seq=seq;
}

MODULE_DEF(os) = {
    .init=os_init,
    .run=os_run,
    .trap=os_trap,
    .on_irq=os_on_irq,
};
