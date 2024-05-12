#include "co.h"
#include <stdlib.h>
#include <ucontext.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#define STACK_SIZE 65536

typedef char uint8_t;

enum co_status {
  CO_NEW = 1, // 新创建，还未执行过
  CO_RUNNING, // 已经执行过
  CO_WAITING, // 在 co_wait 上等待
  CO_DEAD,    // 已经结束，但还未释放资源
};

typedef struct co {
  int num;
  char *name;
  void (*func)(void *); // co_start 指定的入口地址和参数
  void *arg;
  enum co_status status;  // 协程的状态
  struct co * waiter; // 是否有其他协程在等待当前协程
  ucontext_t context; // 寄存器现场
  uint8_t    stack[STACK_SIZE]; // 协程的堆栈
}CO;

CO *coses[129];
CO *current;
int co_cnt=0,allocated=0,cnt=0,dead[129];

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
  if(!allocated){
    allocated=1;
    coses[0]=malloc(sizeof(CO));
    current=coses[0];
    coses[0]->status=CO_RUNNING;
    coses[0]->num=0;
  }
  co_cnt++;
  coses[co_cnt]=malloc(sizeof(CO));
  coses[co_cnt]->num=co_cnt;
  coses[co_cnt]->name=(char *)name;
  coses[co_cnt]->func=func;
  coses[co_cnt]->arg=arg;
  coses[co_cnt]->status=CO_NEW;
  coses[co_cnt]->waiter=NULL;
  getcontext(&coses[co_cnt]->context);
  coses[co_cnt]->context.uc_stack.ss_sp=coses[co_cnt]->stack;
  coses[co_cnt]->context.uc_stack.ss_size=sizeof(coses[co_cnt]->stack);
  coses[co_cnt]->context.uc_stack.ss_flags=0;
  coses[co_cnt]->context.uc_link=&current->context;
  makecontext(&coses[co_cnt]->context,(void(*)())func,1,arg);
  return coses[co_cnt];
}

void co_wait(struct co *co) {
  //printf("CO_WAIT %d\n",co->num);
  if(!allocated){
    allocated=1;
    coses[0]=malloc(sizeof(CO));
    current=coses[0];
    coses[0]->status=CO_RUNNING;
    coses[0]->num=0;
  }
  current->status=CO_WAITING;
  current->waiter=co;
  int temp=current->num;
  //retry:
  assert(coses[co->num]!=NULL);
  while(coses[co->num]->status!=CO_DEAD&&dead[co->num]==0){
    co_yield();
    assert(current->num>=0&&current->num<=co_cnt);
    current=coses[temp];
    current->status=CO_WAITING;
    current->waiter=co;
    //goto retry;
  }
  dead[co->num]=1;
  free(coses[co->num]);
  co=NULL;
  current=coses[temp];
  current->status=CO_RUNNING;
  assert(current->num>=0&&current->num<=co_cnt);
  assert(co==NULL);
}

void co_yield() {
  if(!allocated){
    allocated=1;
    coses[0]=malloc(sizeof(CO));
    current=coses[0];
    coses[0]->status=CO_RUNNING;
    coses[0]->num=0;
  }
  srand(time(0));
  int next=0;
  assert(current!=NULL);
  int now=current->num;
  do{
    retry1:
    next=rand()%(co_cnt+1);
    if(coses[next]==NULL){
      goto retry1;
    }
  }while(next==current->num||dead[next]==1||coses[next]->status==CO_DEAD||coses[next]->status==CO_WAITING);
  int temp=current->num;
  current=coses[next];
  if(coses[now]->status!=CO_WAITING){
    coses[now]->status=CO_RUNNING;
    swapcontext(&coses[now]->context,&coses[next]->context);
    coses[now]->status=CO_DEAD;
    assert(current->num>=0&&current->num<=co_cnt);
    assert(coses[temp]!=NULL);
    current=coses[temp];
  }
  else{
    assert(coses[now]->status==CO_WAITING);
    //printf("yielding as waiter\n");
    assert(coses[next]!=NULL);
    //printf("from:%d to %d\n",now,next);
    swapcontext(&coses[now]->context,&coses[next]->context);
    assert(coses[temp]!=NULL);
    current=coses[temp];
    assert(coses[next]!=NULL);
    assert(coses[now]->status==CO_WAITING);
    //printf("yielded as waiter\n");
    assert(current!=NULL);
    //printf("%d\n",current->num);
    assert(current->waiter!=NULL);
    //printf("%d %d\n",current->waiter->num,coses[next]->num);
    if(current->waiter==coses[next]) coses[next]->status=CO_DEAD,dead[next]=1;
    assert(current->num>=0&&current->num<=co_cnt);
  }
  
}
