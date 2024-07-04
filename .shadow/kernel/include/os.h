#include<kernel.h>
#define RUNNABLE 0
#define RUNNING 1
#define WAIT_SCHEDULE 2
#define WAIT_LOAD 3
#define WAIT_AWAKE 4
#define WAIT_AWAKE_SCHEDULE 5
#define DEAD 6
#define TIMER 5
#define QUESIZ 1024
#define STACK_SIZE 32768

struct cpu {
    int noff;
    int intena;
};

extern struct cpu cpus[];

struct spinlock{
    int locked;
    char name[128];
    int status;
};

struct semaphore{
    int count;
    struct spinlock *lk;
};

struct task{
    int status;
    int remain;
    char name[128];
    Context *context;
    task_t *next,*prev;
    uint8_t stack[STACK_SIZE];
};

