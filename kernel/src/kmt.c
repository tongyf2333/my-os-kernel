#include <common.h>
#define LOCKED 1
#define UNLOCKED 0
#define INT_MIN -2147483647
#define INT_MAX 2147483647
typedef struct Context Context;
struct cpu cpus[64];
struct task *tasks[128];
struct task *current_task[64];
Context *scheduler[64];
spinlock_t lock;
int task_count=0;
int taskcnt=0;
int stepcnt=0;
int rnd=1;
int rand(){
    rnd=48271*rnd%((1<<30)-1);
    return rnd;
}
//linklist
static void insert(task_t *head,task_t *task){
    task_t *prev=head,*next=prev->next;
    task->prev=prev,task->next=next;
    prev->next=task;
    next->prev=task;
    task_count++;
}
static void delete(task_t *head){
    task_t *fwd=head->prev,*nxt=head->next;
    fwd->next=nxt;
    nxt->prev=fwd;
    task_count--;
}
//spinlock
static void kmt_spin_init(spinlock_t *lk, const char *name){
    strcpy(lk->name,name);
    lk->locked=UNLOCKED;
}
static void kmt_spin_lock(spinlock_t *lk){
    int tmp=ienabled();
    iset(false);
    while(atomic_xchg(&lk->locked, LOCKED));
    lk->status=tmp;
}
static void kmt_spin_unlock(spinlock_t *lk){
    assert(!ienabled());
    int tmp=lk->status;
    atomic_xchg(&lk->locked, UNLOCKED);
    iset(tmp);
}
//context saving and scheduling
static Context *kmt_context_save(Event ev, Context *ctx){
    task_t *task=current_task[cpu_current()];
    if(/*task->status==WAIT_AWAKE_SCHEDULE||*/task->status==RUNNING) task->context=ctx;
    else if(task->status==WAIT_LOAD) scheduler[cpu_current()]=ctx;
    return NULL;
}
static Context *irq_timer(Event ev,Context *ctx){
    task_t *task=current_task[cpu_current()];
    if(task->status==RUNNING){
        task->remain--;
        if(task->remain<=0) task->status=WAIT_SCHEDULE;
    }
    return NULL;
}
static Context *sched_yield(Event ev,Context *ctx){
    task_t *task=current_task[cpu_current()];
    if(task->status==RUNNING) task->status=WAIT_SCHEDULE;
    else if(task->status==WAIT_LOAD) task->status=RUNNING;
    return NULL;
}
static Context *kmt_schedule(Event ev, Context *ctx){
    task_t *task=current_task[cpu_current()];
    if(task->status==RUNNING) return task->context;
    else if(/*task->status==WAIT_AWAKE_SCHEDULE||*/task->status==WAIT_SCHEDULE) return scheduler[cpu_current()];
    else return NULL;
}
void solver(){
    while(1){
        yield();
        task_t *task=current_task[cpu_current()];
        kmt_spin_lock(&lock);
        if(task->status==WAIT_SCHEDULE) task->status=RUNNABLE;
        //else if(task->status==WAIT_AWAKE_SCHEDULE) task->status=WAIT_AWAKE;
        kmt_spin_unlock(&lock);
        kmt_spin_lock(&lock);
        task=task->prev;
        int i=1;
        stepcnt=rand()%task_count;
        for(;i<=stepcnt;){
            if(task->status==RUNNABLE) i++;
            task=task->prev;
        }
        while(task->status!=RUNNABLE/*&&task->last_cpu==cpu_current()*/) task=task->prev;
        task->status=WAIT_LOAD;
        task->remain=TIMER;
        task->last_cpu=cpu_current();
        kmt_spin_unlock(&lock);
        current_task[cpu_current()]=task;
    }
}
void solve(){while(1);}
static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg){
    assert(task!=NULL);
    strcpy(task->name,name);
    task->context=kcontext((Area){.start=task->stack,.end=task+1,},entry,arg);
    task->status=RUNNABLE;
    task->remain=TIMER;
    task->last_cpu=-1;
    task->id=++taskcnt;
    kmt->spin_lock(&lock);
    insert(current_task[cpu_current()],task);
    kmt->spin_unlock(&lock);
    return 0;
}
static void kmt_teardown(task_t *task){
    kmt->spin_lock(&lock);
    delete(task);
    pmm->free(task);
    kmt->spin_unlock(&lock);
}
static void kmt_init(){
    kmt_spin_init(&lock,"lock");
    for(int i=0;i<cpu_count();i++){
        current_task[i]=pmm->alloc(sizeof(task_t*));
        scheduler[i]=pmm->alloc(sizeof(Context*));
        task_t *task=pmm->alloc(sizeof(task_t));
        strcpy(task->name,"handler");
        task->status=WAIT_LOAD;
        task->remain=TIMER;
        task->last_cpu=-1;
        task->context=kcontext((Area){.start=task->stack,.end=task+1,},solve,NULL);
        task->prev=task->next=task;
        current_task[i]=task;
    }
    for(int i=1;i<cpu_count();i++){
        insert(current_task[0],current_task[i]);
    }
    os->on_irq(INT_MIN,EVENT_NULL,kmt_context_save);
    os->on_irq(0,EVENT_IRQ_TIMER,irq_timer);
    os->on_irq(0,EVENT_YIELD,sched_yield);
    os->on_irq(INT_MAX,EVENT_NULL,kmt_schedule);
}
//semaphore
static void kmt_sem_init(sem_t *sem, const char *name, int value){
    sem->lk=pmm->alloc(sizeof(spinlock_t));
    kmt_spin_init(sem->lk,name);
    sem->count=value;
}
static void kmt_sem_wait(sem_t *sem){
    int acquire=0;
    while(!acquire){
        kmt_spin_lock(sem->lk);
        if(sem->count>0){
            sem->count--;
            acquire=1;
        }
        kmt_spin_unlock(sem->lk);
        if(!acquire){
            if(ienabled()) yield();
        }
    }
}
static void kmt_sem_signal(sem_t *sem){
    kmt_spin_lock(sem->lk);
    sem->count++;
    kmt_spin_unlock(sem->lk);
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