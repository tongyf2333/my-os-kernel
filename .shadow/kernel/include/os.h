#include<kernel.h>
#define RUNNABLE 0
#define RUNNING 1
#define WAIT_SCHEDULE 2
#define WAIT_LOAD 3
#define WAIT_AWAKE 4
#define WAIT_AWAKE_SCHEDULE 5
#define DEAD 6
#define TIMER 1
#define QUESIZ 1024
#define STACK_SIZE 32768
typedef volatile uintptr_t pthread_mutex;

struct cpu {
    int ncli;
    int intena;
};

extern struct cpu cpus[];

struct spinlock{
    pthread_mutex locked;
    char name[128];
    int status;
    int cpu;
};

struct semaphore{
    int count;
    char *name;
    struct spinlock *lk;
    volatile int waiting_tasks_len;
    volatile task_t** waiting_tasks;
};

struct task{
    int status;
    int remain;
    int id;
    int last_cpu;
    char name[128];
    Context *context;
    task_t *next,*prev;
    uint32_t fence1;
    uint8_t stack[STACK_SIZE];
    uint32_t fence2;
    pthread_mutex block;
    int count;
    pthread_mutex read_write;
    pthread_mutex state;
};

