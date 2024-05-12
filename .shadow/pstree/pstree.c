#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

struct node{
  int to,next;
}e[100005];
int hd[100005],cnt,num_enable=0;
int sta[100005];
bool vis[100005],v[100005];
char nm[100005][20];
int max(int x,int y){
  return x>y?x:y;
}
void addedge(int x,int y){
  e[++cnt].to=y;
  e[cnt].next=hd[x];
  hd[x]=cnt;
}
void dfs(int x,int dep){
  vis[x]=1;
  int tot=0;
  for(int i=hd[x];i;i=e[i].next){
    sta[++tot]=e[i].to;
  }
  for(int i=1;i<=dep;i++) printf("  ");
  printf("%s",nm[x]);
  if(num_enable) printf("(%d)",x);
  printf("\n");
  for(int i=tot;i>=1;i--){
    dfs(sta[i],dep+1);
  }
}

int main(int argc, char *argv[]) {
  int maxnum=0;
  for(int i=1;i<=10000;i++){
    FILE *file;
    char line[100];  
    char filename[100];
    snprintf(filename,sizeof(filename),"/proc/%d/status",i);
    file=fopen(filename,"r");
    if(file==NULL) continue;
    //printf("%s\n",filename);get file names
    int son=0,fa=0;
    while(fgets(line,sizeof(line),file)){
      if(strncmp(line,"Pid:",4)==0){
        int val=0,len=strlen(line);
        for(int j=0;j<len;j++){
          if(line[j]>='0'&&line[j]<='9') val=val*10+(line[j]-'0');
        }
        son=val;
      }
      if(strncmp(line,"PPid:",5)==0){
        int val=0,len=strlen(line);
        for(int j=0;j<len;j++){
          if(line[j]>='0'&&line[j]<='9') val=val*10+(line[j]-'0');
        }
        fa=val;
      }
      if(strncmp(line,"Name:",5)==0){
        int len=strlen(line),tot=0;
        for(int j=6;j<len-1;j++){
          nm[i][tot++]=line[j];
        }
        //printf("pid: %d len: %d\n",i,len);
      }
    }
    addedge(fa,son);
    maxnum=max(maxnum,son);
    v[i]=1;
  }
  int flag=0;
  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
    int len=strlen(argv[i]);
    if(strcmp("-p",argv[i])==0||strcmp("--show-pids",argv[i])==0) num_enable=1;
    if(strcmp("-V",argv[i])==0||strcmp("--version",argv[i])==0) flag=1;
    //printf("argv[%d] = %s\n", i, argv[i]);
  }
  assert(!argv[argc]);
  if(flag){
    fputs("pstree\n",stderr);
  }
  else for(int i=1;i<=maxnum;i++) if(!vis[i]&&v[i]) dfs(i,0);
  return 0;
}
