#include<kernel.h>
#define RUNNABLE 0
#define RUNNING 1
#define BLOCKED 2
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
    char name[128];
    int status;
};

struct semaphore{
    int count;
    struct spinlock *lk;
    queue_t *que;
};

struct task{
    int id;
    int status;
    char name[128];
    void (*entry)(void *);
    struct Context context;
    uint8_t stack[STACK_SIZE];
};

