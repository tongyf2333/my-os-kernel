#include<kernel.h>
#define BLOCKED 2
#define RUNNING 1
#define RUNNABLE 0
#define QUESIZ 256
#define STACK_SIZE 8192

struct cpu {
    int noff;
    int intena;
};

extern struct cpu cpus[];

typedef struct queue{
    task_t *element[QUESIZ];
    int hd,tl;
}queue_t;

struct spinlock{
    int locked;
    const char *name;
    struct cpu *cpu;  
};

struct semaphore{
    int count;
    struct spinlock *lk;
    queue_t *que;
};

struct task{
    int status;
    const char *name;
    struct task *next;
    void (*entry)(void *);
    Context *context;
    uint8_t stack[STACK_SIZE];
};

