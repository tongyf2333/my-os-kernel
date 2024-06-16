#include<kernel.h>
#define BLOCKED 2
#define RUNNING 1
#define RUNNABLE 0
#define DEAD 3
#define QUESIZ 1024
#define STACK_SIZE 32768

struct cpu {
    int noff;
    int intena;
};

extern struct cpu cpus[];

typedef struct queue{
    task_t *element[QUESIZ];
    int hd,tl;
    int cnt;
}queue_t;

struct spinlock{
    int locked;
    int status;
    const char *name;
    int id;
};

struct semaphore{
    int count;
    struct spinlock *lk;
    queue_t *que;
};

struct task{
    struct cpu state;
    int cpuid;
    int id;
    int status;
    const char *name;
    void (*entry)(void *);
    Context *context;
    uint8_t stack[STACK_SIZE];
};

