#include<kernel.h>

struct cpu {
    int noff;
    int intena;
};

extern struct cpu cpus[];

struct spinlock{
    int locked;
    char *name;
    struct cpu *cpu;  
};

struct semaphore{
    int count;
    struct spinlock *lk;
};

struct task{
    const char *name;
    struct task *next;
    void (*entry)(void *);
    Context *context;
    uint8_t *stack;
    char end[0];
};