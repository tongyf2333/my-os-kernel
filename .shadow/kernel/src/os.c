#include <common.h>

static void os_init() {
    pmm->init();
    kmt->init();
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
    Context *next = NULL;
    for (int i=1;i<=cnt;i++) {
        hand h=table[i];
        if (h.event == EVENT_NULL || h.event == ev.event) {
            Context *r = h.handler(ev, ctx);
            //panic_on(r && next, "return to multiple contexts");
            if (r) next = r;
        }
    }
    //panic_on(!next, "return to NULL context");
    //panic_on(sane_context(next), "return to invalid context");
    return next;
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
