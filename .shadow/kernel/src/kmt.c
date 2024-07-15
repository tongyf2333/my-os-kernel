#include <common.h>

#define MAX_CPU_NUM 8
#define INT_MIN 0
#define INT_MAX 255
#define MAX_TASK_NUM 32
#define MAX_CHAR_LEN 128
#define FENCE1 114514233
#define FENCE2 1919810

struct cpu cpus[MAX_CPU_NUM];
static task_t *last[MAX_CPU_NUM];
static task_t *current[MAX_CPU_NUM];
static task_t *idles[MAX_CPU_NUM];
static task_t *tasks[MAX_TASK_NUM];
static unsigned int tasks_len;

static pthread_mutex ktask;
static pthread_mutex kspin;
static pthread_mutex kspin_lock_lock;
static pthread_mutex kspin_unlock_lock;
static pthread_mutex ksem;


inline intptr_t katomic_xchg(volatile pthread_mutex *addr, intptr_t newval) {
    intptr_t result;
    asm volatile ("lock xchg %0, %1":
    "+m"(*addr), "=a"(result) : "1"(newval) : "cc");
    return result;
}

inline void pthread_mutex_lock(pthread_mutex *lock) {
    while (katomic_xchg(lock, 1));
}

inline void pthread_mutex_unlock(pthread_mutex *lock) {
    asm volatile("movl $0, %0" : "+m" (*lock) : );
}

inline int pthread_mutex_trylock(pthread_mutex *lock) {
    return katomic_xchg(lock, 1);
}

void pushcli() {
    iset(false);
    int cpu_id = cpu_current();
    if (cpus[cpu_id].ncli == 0)
        cpus[cpu_id].intena = ienabled();
    cpus[cpu_id].ncli += 1;
}

void popcli() {
    panic_on(ienabled(), "popcli - interruptible");
    int cpu_id = cpu_current();
    panic_on(--cpus[cpu_id].ncli < 0, "popcli");
    if (!cpus[cpu_id].ncli && !cpus[cpu_id].intena) {
        cpus[cpu_id].intena = 1;
        iset(true);
    }
}

static int holding(spinlock_t *lk) {
    int r;
    pushcli();
    r = lk->locked && lk->cpu == cpu_current();
    popcli();
    return r;
}

static void kspin_init(spinlock_t *lk, const char *name) {
    pthread_mutex_lock(&kspin);
    memset(lk->name,0, sizeof(lk->name));
    strcpy(lk->name, name);
    lk->locked = 0;
    lk->cpu = -1;
    pthread_mutex_unlock(&kspin);
}

static void kspin_lock(spinlock_t *lk) {
    pushcli();
    if(holding(lk)) {
        char *info = pmm->alloc(sizeof(MAX_CHAR_LEN));
        strcpy(info, lk->name);
        strcat(info, " acquire");
        panic(info);
    }
    pthread_mutex_lock(&lk->locked);
    lk->cpu = cpu_current();
}

static void kspin_unlock(spinlock_t *lk) {
    if(!holding(lk)) {
        char *info = pmm->alloc(sizeof(MAX_CHAR_LEN));
        strcpy(info, lk->name);
        strcat(info, " release");
        panic(info);
    }
    lk->cpu = -1;
    pthread_mutex_unlock(&lk->locked);
    popcli();
}

static void ksem_init(sem_t *sem, const char *name, int value) {
    pthread_mutex_lock(&ksem);
    memset(sem->name,0, sizeof(sem->name));
    strcpy(sem->name, name);
    sem->count = value;
    sem->waiting_tasks_len = 0;
    sem->lk = pmm->alloc(sizeof(spinlock_t));
    kspin_init(sem->lk, name);
    sem->waiting_tasks = pmm->alloc(MAX_TASK_NUM * sizeof(task_t * ));
    memset(sem->waiting_tasks, 0, sizeof(MAX_TASK_NUM * sizeof(task_t * )));
    pthread_mutex_unlock(&ksem);
}

static void ksem_wait(sem_t *sem) {
    kspin_lock(sem->lk);
    sem->count--;
    if (sem->count < 0) {
        int cpu_id = cpu_current();
        if (current[cpu_id]) {
            sem->waiting_tasks[sem->waiting_tasks_len++] = current[cpu_id];
            current[cpu_id]->block = 1;
        }
    }
    kspin_unlock(sem->lk);
    if (sem->count < 0) {
        yield();
    }
}

static void ksem_signal(sem_t *sem) {
    kspin_lock(sem->lk);
    sem->count++;
    if (sem->count <= 0 && sem->waiting_tasks_len > 0) {
        int r = rand() % sem->waiting_tasks_len;
        sem->waiting_tasks[r]->block = 0;
        for (int i = r; i < sem->waiting_tasks_len - 1; i++)
            sem->waiting_tasks[i] = sem->waiting_tasks[i + 1];
        sem->waiting_tasks_len--;
    }
    kspin_unlock(sem->lk);
}

static void idle(void *arg) {
    while (1);
}

static int kcreate(task_t *task, const char *name, void (*entry)(void *), void *arg) {
    pthread_mutex_lock(&ktask);
    task->fence1 = FENCE1;
    task->fence2 = FENCE2;
    memset(task->name,0,sizeof(task->name));
    strcpy(task->name, name);
    memset(task->stack,0,sizeof(task->stack));
    task->context = kcontext((Area) {(void *) task->stack, (void *) (task->stack + STACK_SIZE)}, entry, arg);
    task->state = 0;
    task->read_write = 0;
    task->count = 0;
    task->id = tasks_len;
    tasks[tasks_len++] = task;
    pthread_mutex_unlock(&ktask);
    return 0;
}

static void kteardown(task_t *task) {
    pthread_mutex_lock(&ktask);
    pmm->free(task);
    tasks_len--;
    pthread_mutex_unlock(&ktask);
}

static Context *kmt_context_save(Event ev, Context *c) {
    int cpu_id = cpu_current();
    if (!current[cpu_id]) {
        current[cpu_id] = idles[cpu_id];
    }
    current[cpu_id]->context = c;
/**
 * Unlock this state after the interruption is down, otherwise, the stack would be corrupted.
 * One way is unlock the state in the second interruption.
 */
    if(last[cpu_id] && last[cpu_id] != current[cpu_id])
        pthread_mutex_unlock(&last[cpu_id]->state);
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
            //tasks[i]->block means blocked because waiting sem or spinlock
            //trylock means trying to get the lock

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

    tasks_len = 0;
    ktask = 0;
    kspin = 0;
    kspin_lock_lock = 0;
    kspin_unlock_lock = 0;
    ksem = 0;
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
        cpus[i].ncli = 0;
        cpus[i].intena = 1;
    }
}

MODULE_DEF(kmt) = {
    .init  = kmt_init,
    .create = kcreate,
    .teardown  = kteardown,
    .spin_init = kspin_init,
    .spin_lock = kspin_lock,
    .spin_unlock = kspin_unlock,
    .sem_init = ksem_init,
    .sem_wait = ksem_wait,
    .sem_signal = ksem_signal
};