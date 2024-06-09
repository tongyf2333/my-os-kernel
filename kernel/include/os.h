#include<kernel.h>

struct spinlock{

};

struct semaphore{

};

struct task{
    const char *name;
    struct task *next;
    void (*entry)(void *);
    Context *context;
    uint8_t *stack;
};