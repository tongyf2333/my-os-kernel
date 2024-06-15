#include <common.h>
#define LOCKED 1
#define UNLOCKED 0
#define INT_MIN -2147483647
#define INT_MAX 2147483647

struct cpu cpus[64];

struct task *tasks[128];
struct task *current_task[16];
int task_count=0;

spinlock_t lock;
queue_t global;

extern void solver();

static void enqueue(queue_t *q,task_t *elem){
    q->element[((q->tl)+1)%QUESIZ]=elem;
    q->tl=((q->tl)+1)%QUESIZ;
    q->cnt++;
}

static task_t *dequeue(queue_t *q){
    task_t *res=q->element[q->hd];
    q->hd=((q->hd)+1)%QUESIZ;
    q->cnt--;
    return res;
}

static struct cpu *mycpu(){
    return &cpus[cpu_current()];
}

bool holding(spinlock_t *lk) {
    assert(!ienabled());
    return (
        lk->locked == LOCKED &&
        lk->cpu == mycpu()
    );
}
void push_off(void) {
    int old = ienabled();
    iset(false);
    if(mycpu()->noff == 0)
        mycpu()->intena = old;
    mycpu()->noff += 1;
}

void pop_off(void) {
    if(ienabled())
        panic("pop_off - interruptible");
    if(mycpu()->noff < 1)
        panic("pop_off");
    mycpu()->noff -= 1;
    if(mycpu()->noff == 0 && mycpu()->intena)
        iset(true);
}

static void kmt_spin_lock(spinlock_t *lk){
    push_off();
    if (holding(lk)){
        panic("deadlock!");
    }
    int got;
    do{
        got=atomic_xchg(&lk->locked, LOCKED);
    }while (got != UNLOCKED);
    lk->cpu=mycpu();
}

static void kmt_spin_unlock(spinlock_t *lk){
    if (!holding(lk)){
        printf("release:%s\n",lk->name);
        panic("double release");//bang!
        assert(0);
    }
    lk->cpu = NULL;
    atomic_xchg(&lk->locked, UNLOCKED);
    pop_off();
}

static Context *kmt_context_save(Event ev, Context *ctx){
    if(ev.event!=EVENT_YIELD){
        kmt_spin_lock(&lock);
        enqueue(&global,current_task[cpu_current()]);
        kmt_spin_unlock(&lock);
    }
    if (current_task[cpu_current()]==NULL){
        kmt_spin_lock(&lock);
        while(global.cnt<=0){
            kmt_spin_unlock(&lock);
            kmt_spin_lock(&lock);
        }
        current_task[cpu_current()] = dequeue(&global);
        kmt_spin_unlock(&lock);
    }
    else current_task[cpu_current()]->context = ctx;
    return NULL;
}
static Context *kmt_schedule(Event ev, Context *ctx){//bug here
    kmt_spin_lock(&lock);
    while(global.cnt<=0){
        kmt_spin_unlock(&lock);
        kmt_spin_lock(&lock);
    }
    current_task[cpu_current()] = dequeue(&global);
    kmt_spin_unlock(&lock);
    return current_task[cpu_current()]->context;
}

static void kmt_teardown(task_t *task){
    pmm->free(task->context);
    task->status=DEAD;
}

static void kmt_spin_init(spinlock_t *lk, const char *name){
    lk->name=name;
    lk->locked=UNLOCKED;
    lk->cpu=NULL;
}

static void kmt_sem_init(sem_t *sem, const char *name, int value){
    sem->lk=pmm->alloc(sizeof(spinlock_t));
    kmt_spin_init(sem->lk,name);
    sem->count=value;
    sem->que=pmm->alloc(sizeof(queue_t));
    sem->que->hd=0;
    sem->que->tl=-1;
    sem->que->cnt=0;
}
static void kmt_sem_wait(sem_t *sem){
    /*int acquired=0;
    kmt_spin_lock(sem->lk);
    if (sem->count<=0) {
        current_task[cpu_current()]->status = BLOCKED;
        enqueue(sem->que, current_task[cpu_current()]);
        
    } 
    else {
        sem->count--;
        acquired = 1;
    }
    kmt_spin_unlock(sem->lk);
    if (!acquired) yield();*/
    kmt_spin_lock(sem->lk);
    sem->count--;
    if(sem->count<0){
        task_t *now=current_task[cpu_current()];
        now->status = BLOCKED;
        enqueue(sem->que, now);
        while(now->status==BLOCKED){
            kmt_spin_unlock(sem->lk);
            yield();
            kmt_spin_lock(sem->lk);
        }
    }
    kmt_spin_unlock(sem->lk);
}
static void kmt_sem_signal(sem_t *sem){
    /*kmt_spin_lock(sem->lk);
    if((sem->que->cnt)>0) {
        task_t *task = dequeue(sem->que);
        task->status = RUNNING;
    } 
    else sem->count++;
    kmt_spin_unlock(sem->lk);*/
    kmt_spin_lock(sem->lk);
    sem->count++;
    if(sem->count<=0){
        if(sem->que->cnt>0){
            task_t *now=dequeue(sem->que);
            if(now){
                now->status=RUNNING;
                kmt_spin_lock(&lock);
                enqueue(&global,now);
                kmt_spin_unlock(&lock);
            }
        }
    }
    kmt_spin_unlock(sem->lk);
}

static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg){
    kmt_spin_lock(&lock);
    task->entry=entry;
    task->name=name;
    task->status=RUNNING;
    task->context=kcontext((Area){.start=task->stack,.end=task+1,},entry,arg);
    task->id=task_count;
    tasks[task_count]=task;
    task_count++;
    enqueue(&global,task);
    kmt_spin_unlock(&lock);
    return 0;
}

static void kmt_init(){
    kmt_spin_init(&lock,"lock");
    task_count=0;
    for(int i=0;i<cpu_count();i++) cpus[i].noff=0,current_task[i]=NULL;
    kmt_create(pmm->alloc(sizeof(task_t)),"irq",solver,NULL);
    os->on_irq(INT_MIN,EVENT_NULL,kmt_context_save);
    os->on_irq(INT_MAX,EVENT_NULL,kmt_schedule);
}


MODULE_DEF(kmt) = {
    .init=kmt_init,
    .create=kmt_create,
    .teardown=kmt_teardown,
    .spin_init=kmt_spin_init,
    .spin_lock=kmt_spin_lock,
    .spin_unlock=kmt_spin_unlock,
    .sem_init=kmt_sem_init,
    .sem_signal=kmt_sem_signal,
    .sem_wait=kmt_sem_wait,
};