#include <common.h>

void init(){

}
int create(task_t *task, const char *name, void (*entry)(void *arg), void *arg){

}
void teardown(task_t *task){

}
void spin_init(spinlock_t *lk, const char *name){

}
void spin_lock(spinlock_t *lk){

}
void spin_unlock(spinlock_t *lk){

}
void sem_init(sem_t *sem, const char *name, int value){

}
void sem_wait(sem_t *sem){

}
void sem_signal(sem_t *sem){
    
}