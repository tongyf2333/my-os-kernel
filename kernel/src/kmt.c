#include <common.h>
#define STACK_SIZE 8192
#define LOCKED 1
#define UNLOCKED 0

struct cpu cpus[16];

struct task *tasks[128],*current_task;
int task_count=0;


static void enqueue(queue_t *q,task_t *elem){
    if(q->tl==QUESIZ) q->tl=0;
    q->element[++(q->tl)]=elem;
}

static task_t *dequeue(queue_t *q){
    return q->element[(q->hd)++];
}

static struct cpu *mycpu(){
    return &cpus[cpu_current()];
}

bool holding(spinlock_t *lk) {
    return (
        lk->locked == LOCKED &&
        lk->cpu == &cpus[cpu_current()]
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
    struct cpu *c = mycpu();
    if(ienabled())
        panic("pop_off - interruptible");
    if(c->noff < 1)
        panic("pop_off");
    c->noff -= 1;
    if(c->noff == 0 && c->intena)
        iset(true);
}


static void kmt_init(){
    current_task=NULL;
}
static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg){
    task->stack=pmm->alloc(STACK_SIZE);
    task->entry=entry;
    task->name=name;
    task->status=RUNNING;
    task->context=kcontext((Area){.start=&task->end,.end=task+1,},task->entry,arg);
    tasks[task_count++]=task;
    if(task_count!=1) task->next=tasks[0],tasks[task_count-2]->next=tasks[task_count-1];
    return 0;
}
static void kmt_teardown(task_t *task){
    pmm->free(task->stack);
}
static void kmt_spin_init(spinlock_t *lk, const char *name){
    lk->name=name;
    lk->locked=0;
    lk->cpu=0;
}
static void kmt_spin_lock(spinlock_t *lk){
    push_off();
    if (holding(lk)) panic("deadlock!");
    int got;
    do{
        got=atomic_xchg(&lk->locked, LOCKED);
    }while (got != UNLOCKED);
    lk->cpu=mycpu();
}
static void kmt_spin_unlock(spinlock_t *lk){
    if (!holding(lk)) {
        panic("double release");
    }
    lk->cpu = NULL;
    atomic_xchg(&lk->locked, UNLOCKED);
    pop_off();
}
static void kmt_sem_init(sem_t *sem, const char *name, int value){
    kmt_spin_init(sem->lk,name);
    sem->count=value;
    sem->que=pmm->alloc(sizeof(queue_t));
    sem->que->hd=1;
    sem->que->tl=0;

}
static void kmt_sem_wait(sem_t *sem){
    /*int acquired = 0;
    kmt_spin_lock(sem->lk);
    if (sem->count <= 0) {
        enqueue(sem->que, current_task);
        current_task->status = BLOCKED;
    }
    else{
        sem->count--;
        acquired = 1;
    }
    kmt_spin_unlock(sem->lk);
    if (!acquired) yield();*/
    kmt_spin_lock(sem->lk);
    while(sem->count<=0){
        enqueue(sem->que,current_task);
        current_task->status=BLOCKED;
        yield();
    }
    sem->count--;
    kmt_spin_unlock(sem->lk);
}
static void kmt_sem_signal(sem_t *sem){
    /*kmt_spin_lock(sem->lk);
    if((sem->que->hd)<=(sem->que->tl)) {
        task_t *task = dequeue(sem->que);
        task->status = RUNNABLE;
    } 
    else{
        sem->count++;
    }
    kmt_spin_unlock(sem->lk);*/
    kmt_spin_lock(sem->lk);
    sem->count++;
    if((sem->que->hd)<=(sem->que->tl)) {
        task_t *task = dequeue(sem->que);
        task->status = RUNNABLE;
    } 
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