#include <common.h>

struct cpu cpus[MAX_CPU_NUM];
static task_t *last[MAX_CPU_NUM],*idles[MAX_CPU_NUM],*tasks[MAX_TASK_NUM];
task_t *current[MAX_CPU_NUM];
static unsigned int tasks_len;

static pthread_mutex ktask,kspin,ksem;

int taskcnt=0;

//mini spinlock
inline void pthread_mutex_lock(pthread_mutex *lock) {while (atomic_xchg((int*)lock, 1));}
inline void pthread_mutex_unlock(pthread_mutex *lock) {atomic_xchg((int*)lock,0);}
inline int pthread_mutex_trylock(pthread_mutex *lock) {return atomic_xchg((int*)lock, 1);}
//spinlock
bool holding(spinlock_t *lk) {
    return (
        lk->status == LOCKED &&
        lk->cpu == cpu_current()
    );
}

void push_off(void) {
    int old = ienabled();
    iset(false);
    if (cpus[cpu_current()].noff == 0) cpus[cpu_current()].intena = old;
    cpus[cpu_current()].noff += 1;
}

void pop_off(void) {
    struct cpu *c = &cpus[cpu_current()];
    if (ienabled()) panic("pop_off - interruptible");
    if (c->noff < 1) panic("pop_off");
    c->noff -= 1;
    if (c->noff == 0 && c->intena) iset(true);
}

void spin_lock(spinlock_t *lk) {
    push_off();
    if (holding(lk)) panic("acquire");
    int got;
    do {
        got = atomic_xchg(&lk->status, LOCKED);
    } while (got != UNLOCKED);
    lk->cpu = cpu_current();
}

void spin_unlock(spinlock_t *lk) {
    if (!holding(lk)) panic("release");
    lk->cpu = -1;
    atomic_xchg(&lk->status, UNLOCKED);
    pop_off();
}

static void spin_init(spinlock_t *lk, const char *name) {
    pthread_mutex_lock(&kspin);
    memset(lk->name,0, sizeof(lk->name));
    strcpy(lk->name, name);
    lk->locked = 0;
    lk->cpu = -1;
    pthread_mutex_unlock(&kspin);
}

//semaphore
static void sem_init(sem_t *sem, const char *name, int value) {
    pthread_mutex_lock(&ksem);
    memset(sem->name,0, sizeof(sem->name));
    strcpy(sem->name, name);
    sem->count = value;
    sem->waiting_tasks_len = 0;
    sem->lk = pmm->alloc(sizeof(spinlock_t));
    spin_init(sem->lk, name);
    sem->waiting_tasks = pmm->alloc(MAX_TASK_NUM * sizeof(task_t * ));
    memset(sem->waiting_tasks, 0, sizeof(MAX_TASK_NUM * sizeof(task_t * )));
    pthread_mutex_unlock(&ksem);
}

static void sem_wait(sem_t *sem) {
    spin_lock(sem->lk);
    sem->count--;
    if (sem->count < 0) {
        int cpu_id = cpu_current();
        if (current[cpu_id]) {//push into the waiting list
            sem->waiting_tasks[sem->waiting_tasks_len++] = current[cpu_id];
            current[cpu_id]->block = 1;
        }
    }
    spin_unlock(sem->lk);
    if (sem->count < 0)  yield();
}

static void sem_signal(sem_t *sem) {
    spin_lock(sem->lk);
    sem->count++;
    if (sem->count <= 0 && sem->waiting_tasks_len > 0) {//randomly choose a thread waiting this sem
        int r = rand() % sem->waiting_tasks_len;
        sem->waiting_tasks[r]->block = 0;
        for (int i = r; i < sem->waiting_tasks_len - 1; i++)
            sem->waiting_tasks[i] = sem->waiting_tasks[i + 1];
        sem->waiting_tasks_len--;
    }
    spin_unlock(sem->lk);
}
//thread management
static void idle(void *arg) {while (1);}

static int create(task_t *task, const char *name, void (*entry)(void *), void *arg) {
    pthread_mutex_lock(&ktask);
    task->fence1 = FENCE1;
    task->fence2 = FENCE2;
    memset(task->name,0,sizeof(task->name));
    strcpy(task->name, name);
    memset(task->stack,0,sizeof(task->stack));
    task->context = kcontext((Area) {(void *) task->stack, (void *) (task->stack + STACK_SIZE)}, entry, arg);
    task->state = 0;
    task->count = 0;
    task->id = tasks_len;
    task->cnt=++taskcnt;
    tasks[tasks_len++] = task;
    pthread_mutex_unlock(&ktask);
    return 0;
}

static void teardown(task_t *task) {
    pthread_mutex_lock(&ktask);
    pmm->free(task);
    tasks_len--;
    pthread_mutex_unlock(&ktask);
}

static Context *kmt_context_save(Event ev, Context *c) {
    int cpu_id = cpu_current();
    if (!current[cpu_id]) current[cpu_id] = idles[cpu_id];
    current[cpu_id]->context = c;
    //must unlock a thread when out of interruption
    if(last[cpu_id] && last[cpu_id] != current[cpu_id])
        pthread_mutex_unlock(&last[cpu_id]->state);//unlock the last thread(avoid stack data race)
    last[cpu_id] = current[cpu_id];//last[cpuid] means the last thread this cpu runs
    panic_on(current[cpu_id]->fence1 != FENCE1 || current[cpu_id]->fence2 != FENCE2, "stackoverflow");
    return NULL;
}

static Context *kmt_schedule(Event ev, Context *c) {
    int cpu_id = cpu_current();
    task_t *task_interrupt = idles[cpu_id];
    unsigned int i = -1;
    for (int _ = 0; _ < tasks_len * 10; _++) {
        i = rand() % tasks_len;
        if (!tasks[i]->block && (tasks[i] == current[cpu_id] || !pthread_mutex_trylock(&tasks[i]->state))) {
            //tasks[i]->block means blocked because waiting sem or spinlock.
            //trylock means trying to get the lock.
            //go to a thread who doesn't wait on sem/spinlock ,the thread can be the original thread or an unlocked thread.
            task_interrupt = tasks[i];
            break;
        }
    }
    current[cpu_id] = task_interrupt;
    panic_on(task_interrupt->fence1 != FENCE1 || task_interrupt->fence2 != FENCE2, "stackoverflow");
    return current[cpu_id]->context;
}

static void kmt_init() {
    os->on_irq(INT_MIN, EVENT_NULL, kmt_context_save);
    os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
    tasks_len = 0,ktask = 0,kspin = 0,ksem = 0;
    for (int i = 0; i < cpu_count(); i++) {
        idles[i] = pmm->alloc(sizeof(task_t));
        idles[i]->id = -1;
        memset(idles[i]->name,0,sizeof(idles[i]->name));
        strcpy(idles[i]->name,"idle");
        memset(idles[i]->stack,0,sizeof(idles[i]->stack));
        idles[i]->context = kcontext((Area) {(void *) idles[i]->stack, (void *) (idles[i]->stack + STACK_SIZE)}, idle, NULL);
        idles[i]->state = 0;
        idles[i]->fence1 = FENCE1;
        idles[i]->fence2 = FENCE2;
        current[i] = NULL;
        last[i] = NULL;
        cpus[i].noff = 0;
        cpus[i].intena = 1;
    }
}

MODULE_DEF(kmt) = {
    .init  = kmt_init,
    .create = create,
    .teardown  = teardown,
    .spin_init = spin_init,
    .spin_lock = spin_lock,
    .spin_unlock = spin_unlock,
    .sem_init = sem_init,
    .sem_wait = sem_wait,
    .sem_signal = sem_signal
};