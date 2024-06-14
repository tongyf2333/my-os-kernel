#include <common.h>
#define LOCKED 1
#define UNLOCKED 0
#define INT_MIN -2147483647
#define INT_MAX 2147483647

struct cpu cpus[16];

struct task *tasks[128];
struct task *current_task[16];
int task_count=0;

spinlock_t lock;

void solver(void *arg){while(1);}


static Context *kmt_context_save(Event ev, Context *ctx){
    if (current_task[cpu_current()]==NULL) current_task[cpu_current()] = tasks[0];
    else current_task[cpu_current()]->context = ctx;
    return NULL;
}
static Context *kmt_schedule(Event ev, Context *ctx){//bug here
    do {
        current_task[cpu_current()] = current_task[cpu_current()]->next;
    } while (
        current_task[cpu_current()]->status != RUNNING ||
        ((current_task[cpu_current()]->cpu_id!=-1)&&(current_task[cpu_current()]->cpu_id!=cpu_current()))
    );
    current_task[cpu_current()]->cpu_id=cpu_current();
    return current_task[cpu_current()]->context;
}

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
        panic("double release");//bang!
    }
    lk->cpu = NULL;
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
    int acquired=0;
    kmt_spin_lock(sem->lk);
    if (sem->count<=0) {
        enqueue(sem->que, current_task[cpu_current()]);
        current_task[cpu_current()]->status = BLOCKED;
    } else {
        sem->count--;
        acquired = 1;
    }
    kmt_spin_unlock(sem->lk);
    if (!acquired) yield();
}
static void kmt_sem_signal(sem_t *sem){
    kmt_spin_lock(sem->lk);
    if((sem->que->cnt)>0) {
        task_t *task = dequeue(sem->que);
        task->status = RUNNING;
    } 
    else sem->count++;
    kmt_spin_unlock(sem->lk);
}

static void kmt_init(){
    kmt_spin_init(&lock,"null");
    task_count=0;
    for(int i=0;i<8;i++) cpus[i].noff=0,current_task[i]=NULL;
    kmt->create(pmm->alloc(sizeof(task_t)),"irq",solver,NULL);
    os->on_irq(INT_MIN,EVENT_NULL,kmt_context_save);
    os->on_irq(INT_MAX,EVENT_NULL,kmt_schedule);
}

static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg){
    kmt_spin_lock(&lock);
    task->entry=entry;
    task->name=name;
    task->status=RUNNING;
    task->cpu_id=-1;
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