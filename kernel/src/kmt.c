#include <common.h>
#define STACK_SIZE 8192
#define LOCKED 1
#define UNLOCKED 0

task_t *head;

struct cpu cpus[16];

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
    int old = intr_get();
    intr_off();
    if(mycpu()->noff == 0)
        mycpu()->intena = old;
    mycpu()->noff += 1;
}

void pop_off(void) {
    struct cpu *c = mycpu();
    if(intr_get())
        panic("pop_off - interruptible");
    if(c->noff < 1)
        panic("pop_off");
    c->noff -= 1;
    if(c->noff == 0 && c->intena)
        intr_on();
}


static void kmt_init(){
    head=NULL;
}
static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg){
    task->stack=pmm->alloc(STACK_SIZE);
    task->entry=entry;
    task->name=name;
    task->context=kcontext((Area){.start=&task->end,.end=task+1,},task->entry,arg);
    task->next=head;
    head=task;
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
    lk->cpu=cpu_current();
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

}
static void kmt_sem_wait(sem_t *sem){

}
static void kmt_sem_signal(sem_t *sem){

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