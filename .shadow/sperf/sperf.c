#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

double tim[4096];
int tot;
char cur[4096];

typedef struct node{
    char *name;
    double time;
}node;
node table[10005],temp[10005];
int cnt;
int cmp(node a,node b){
    return a.time>b.time;
}
void add(const char *str,double val){
    printf("QAQ\n");
    int find=0;
    for(int i=1;i<=cnt;i++){
        if(strcmp(str,table[i].name)==0){
            table[i].time+=val;
            find=1;
            break;
        }
    }
    if(!find){
        table[++cnt].name=(char *)str;
        table[cnt].time=val;
    }
}
void merge(int l,int r){
	if(l==r) return;
	int mid=(l+r)/2;
	merge(l,mid);
	merge(mid+1,r);
	int hd=l,tl=mid+1,now=l;
	while(hd<=mid&&tl<=r){
		if(cmp(table[hd],table[tl])) temp[now++]=table[hd++];
		else temp[now++]=table[tl++];
	}
	while(hd<=mid) temp[now++]=table[hd++];
	while(tl<=r) temp[now++]=table[tl++];
	for(int i=l;i<=r;i++) table[i]=temp[i];
}


void gettim(const char *str) {
    char bf[4096];
    regex_t regex;
    int reti;
    int match=2;
    regmatch_t mch[5];
    reti = regcomp(&regex, "<(-?[0-9]+(\\.[0-9]+)?)>", REG_EXTENDED);
    reti = regexec(&regex, str, match, mch, 0);
    if(reti==REG_NOMATCH) return;
    for (int i=0;i<match&&mch[i].rm_so!=-1;i++){
        strncpy(bf, str+mch[i].rm_so, mch[i].rm_eo-mch[i].rm_so);
        bf[mch[i].rm_eo-mch[i].rm_so]=0;
        double db=atof(bf);
        if(db!=0.0){
            tim[++tot]=db;
            add(cur,tim[tot]);
        }
    }
    regfree(&regex);
}

void getname(const char *str){
    regex_t regex;
    int reti;
    int match=2;
    regmatch_t mch[5];
    printf("QAQ\n");
    reti = regcomp(&regex, "^\\w+(?=\\()", REG_EXTENDED);
    printf("QAQ\n");
    reti = regexec(&regex, str, match, mch, 0);
    if(reti==REG_NOMATCH) return;
    for (int i=0;i<match&&mch[i].rm_so!=-1;i++){
        printf("QAQ\n");
        strncpy(cur, str+mch[i].rm_so, mch[i].rm_eo-mch[i].rm_so);
        //printf("name:%s\n",cur);
    }
    regfree(&regex);
}

int main(int argc, char *argv[]) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    __pid_t id=fork();
    if(!id){
        close(pipefd[0]);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        char *exec_argv[4096];
        exec_argv[0]="strace";
        exec_argv[1]="-T";
        for(int i=1;i<argc;i++){
            exec_argv[i+1]=argv[i];
        }
        char *exec_envp[]={"PATH=/bin",NULL,};
        execve("/bin/strace",exec_argv, exec_envp);
        
    }
    else{
        close(pipefd[1]);
        char buffer[4096];
        ssize_t bytesRead;
        while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            write(STDOUT_FILENO, buffer, bytesRead);
            printf("QAQ\n");
            getname(buffer);
            printf("QAQ\n");
            gettim(buffer);
            memset(buffer,0,sizeof(buffer));
            fflush(stdout);
        }
        close(pipefd[0]);
        printf("%d\n",cnt);
        for(int i=1;i<=cnt;i++) printf("%s %lf\n",table[i].name,table[i].time);
    }
    return 0;
}
