#include<kernel.h>
#define TIMER 1
#define STACK_SIZE 64*1024
#define MAX_CPU_NUM 8
#define INT_MIN 0
#define INT_MAX 255
#define MAX_TASK_NUM 32
#define MAX_CHAR_LEN 128
#define FENCE1 114514233
#define FENCE2 1919810
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
    int id;
    int cnt;
    char name[128];
    Context *context;
    uint32_t fence1;
    uint8_t stack[STACK_SIZE];
    uint32_t fence2;
    pthread_mutex block;
    int count;
    pthread_mutex read_write;
    pthread_mutex state;
};

