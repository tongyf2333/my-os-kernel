#include <common.h>
#define LOCKED 1
#define UNLOCKED 0
#define INT_MIN -2147483647
#define INT_MAX 2147483647
typedef struct Context Context;
struct cpu cpus[64];
struct task *tasks[128];
struct task *current_task[64];
spinlock_t lock,irq;
int task_count=0;
void enqueue(queue_t *q,task_t *elem){
    q->element[((q->tl)+1)%QUESIZ]=elem;
    q->tl=((q->tl)+1)%QUESIZ;
    q->cnt++;
}
task_t *dequeue(queue_t *q){
    task_t *res=q->element[q->hd];
    q->hd=((q->hd)+1)%QUESIZ;
    q->cnt--;
    return res;
}
static struct cpu *mycpu(){return &cpus[cpu_current()];}
bool holding(spinlock_t *lk) {
    assert(!ienabled());
    return (lk->locked == LOCKED &&lk->id==current_task[cpu_current()]->id);
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
    while(atomic_xchg(&lk->locked, LOCKED)){
        if(ienabled()) yield();
    }
    push_off();
    lk->id=cpu_current();
}
static void kmt_spin_init(spinlock_t *lk, const char *name){
    strcpy(lk->name,name);
    lk->locked=UNLOCKED;
    lk->id=-1;
}
static void kmt_spin_unlock(spinlock_t *lk){
    atomic_xchg(&lk->locked, UNLOCKED);
    lk->id=-1;
    pop_off();
}
static Context *kmt_context_save(Event ev, Context *ctx){
    current_task[cpu_current()]->context=*ctx;
    current_task[cpu_current()]->status=RUNNABLE;
    return NULL;
}
static Context *kmt_schedule(Event ev, Context *ctx){
    kmt->spin_lock(&lock);
    int start=0;
    if(current_task[cpu_current()]->id<cpu_count()){
        start=cpu_count();
        while(1){
            if(tasks[start]!=NULL){
                if(tasks[start]->status!=BLOCKED&&tasks[start]->status!=RUNNING) break;
            }
            if(start==task_count-1) start=cpu_count();
            else start++;
        }
    }
    else{
        start=0;
        while(1){
            if(tasks[start]!=NULL){
                if(tasks[start]->status!=BLOCKED&&tasks[start]->status!=RUNNING) break;
            }
            if(start==task_count-1) start=0;
            else start++;
        }
    }
    //if(current_task[cpu_current()]->id==task_count-1) start=0;
    //else start=current_task[cpu_current()]->id+1;
    //printf("[%d->%d]",current_task[cpu_current()]->id+1,start+1);
    kmt->spin_unlock(&lock);
    current_task[cpu_current()]=tasks[start];
    current_task[cpu_current()]->status=RUNNING;
    assert(&(current_task[cpu_current()]->context)!=NULL);
    return &(current_task[cpu_current()]->context);
}
static void kmt_teardown(task_t *task){
    
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
    /*int acquire=0;
    kmt_spin_lock(sem->lk);
    if(sem->count>0){
        sem->count--;
        acquire=1;
    }
    else{
        task_t *cur=current_task[cpu_current()];
        cur->status=BLOCKED;
        enqueue(sem->que,cur);
    }
    kmt_spin_unlock(sem->lk);
    if(!acquire){
        if(ienabled()) yield();
    }*/
}
static void kmt_sem_signal(sem_t *sem){
    kmt_spin_lock(sem->lk);
    sem->count++;
    kmt_spin_unlock(sem->lk);
    /*kmt_spin_lock(sem->lk);
    if(sem->que->cnt>0){
        task_t *cur=dequeue(sem->que);
        cur->status=RUNNABLE;
    }
    else sem->count++;
    kmt_spin_unlock(sem->lk);*/
}
static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg){
    assert(task!=NULL);
    kmt->spin_lock(&lock);
    task->entry=entry;
    strcpy(task->name,name);
    task->status=RUNNABLE;
    task->context=*kcontext((Area){.start=task->stack,.end=task+1,},entry,arg);
    task->id=task_count;
    tasks[task_count]=task;
    task_count++;
    kmt->spin_unlock(&lock);
    return 0;
}
static void kmt_init(){
    task_count=0;
    for(int i=0;i<cpu_count();i++) cpus[i].noff=0,cpus[i].intena=0,current_task[i]=NULL;
    for(int i=0;i<128;i++) tasks[i]=NULL;
    os->on_irq(INT_MIN,EVENT_NULL,kmt_context_save);
    os->on_irq(INT_MAX,EVENT_NULL,kmt_schedule);
    kmt->spin_init(&lock,"lock");
    kmt->spin_init(&irq,"irq");
    for(int i=0;i<cpu_count();i++){
        task_t *t=pmm->alloc(sizeof(task_t));
        kmt->create(t,"null",NULL,NULL);
        current_task[i]=t;
        t->status=RUNNING;
    }
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