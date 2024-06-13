#include <common.h>
#define LOCKED 1
#define UNLOCKED 0

struct cpu cpus[16];

struct task *tasks[128],*current_task;
int task_count=0;

spinlock_t lock;

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
    assert(!ienabled());//bang!
    return (
        lk->locked == LOCKED &&
        lk->cpu == mycpu()
    );
}
void push_off(void) {
    int old = ienabled();
    iset(false);
    //iset_false();
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
        //iset_true();
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
static void kmt_spin_lock(spinlock_t *lk){
    push_off();
    if (holding(lk)){
        //printf("name:%s\n",lk->name);
        panic("deadlock!");
    }
    int got;
    do{
        got=atomic_xchg(&lk->locked, LOCKED);
    }while (got != UNLOCKED);
    __sync_synchronize();
    lk->cpu=mycpu();
}
static void kmt_spin_unlock(spinlock_t *lk){
    if (!holding(lk)){
        //printf("name:%s\n",lk->name);
        panic("double release");//bang!
    }
    lk->cpu = NULL;
    __sync_synchronize();
    atomic_xchg(&lk->locked, UNLOCKED);
    pop_off();
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
    //printf("P:%s at cpu:%d\n",sem->lk->name,cpu_current()+1);
    kmt_spin_lock(sem->lk);
    if(sem->count>0){
        sem->count--;
        kmt_spin_unlock(sem->lk);
    }
    else{
        enqueue(sem->que,current_task);
        current_task->status=BLOCKED;
        kmt_spin_unlock(sem->lk);
        while(current_task->status!=RUNNING){
            yield();
            __sync_synchronize();
        }
    }
}
static void kmt_sem_signal(sem_t *sem){
    //printf("V:%s at cpu:%d\n",sem->lk->name,cpu_current()+1);
    kmt_spin_lock(sem->lk);
    if((sem->que->cnt)>0) {
        task_t *task = dequeue(sem->que);
        task->status = RUNNING;
    } 
    else sem->count++;
    kmt_spin_unlock(sem->lk);
}

static void kmt_init(){
    current_task=NULL;
    kmt_spin_init(&lock,"null");
    task_count=0;
    for(int i=0;i<8;i++) cpus[i].noff=0;
}

static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg){
    kmt_spin_lock(&lock);
    task->entry=entry;
    task->name=name;
    task->status=RUNNING;
    assert((intptr_t)(&task->stack)==(intptr_t)(task->stack));
    task->context=kcontext((Area){.start=task->stack,.end=task+1,},entry,arg);
    task->id=task_count;
    tasks[task_count]=task;
    task_count++;
    task->next=tasks[0];
    if(task_count!=1) tasks[task_count-2]->next=task;
    kmt_spin_unlock(&lock);
    return 0;
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