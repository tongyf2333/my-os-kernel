#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <time.h>
#include <dlfcn.h>

const char *nul="\0";
const double eps=1e-9;
double sum,curtime,lasttime=0.0;
int tot;
char cur[4096];

typedef struct node{
    char *name;
    double time;
}node;
node table[10005],temp[10005];
int cnt=0;
int cmp(node a,node b){
    return a.time-b.time>eps;
}
void add(double val){
    int find=0;
    for(int i=1;i<=cnt;i++){
        if(strcmp(cur,table[i].name)==0){
            table[i].time+=val;
            find=1;
            break;
        }
    }
    if(!find){
        table[++cnt].name=malloc(4096);
        strcpy(table[cnt].name,cur);
        table[cnt].time=val;
    }
    sum+=val;
    //printf("name:%s time:%lf\n",cur,val);
    //fflush(stdout);
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
            add(db);
        }
    }
    regfree(&regex);
}

void getname(const char *str){
    if((str[0]<'0')||(str[0]>'9')) return;
    char bf[4096];
    regex_t regex;
    int reti;
    int match=2;
    regmatch_t mch[5];
    reti = regcomp(&regex, "(-?[0-9]+(\\.[0-9]+)?)", REG_EXTENDED);
    reti = regexec(&regex, str, match, mch, 0);
    if(reti==REG_NOMATCH) return;
    int i;
    for (i=0;i<1&&mch[i].rm_so!=-1;i++){
        strncpy(bf, str+mch[i].rm_so, mch[i].rm_eo-mch[i].rm_so);
        bf[mch[i].rm_eo-mch[i].rm_so]=0;
        double db=atof(bf);
        if(db!=0.0){
            if(lasttime==0.0) lasttime=db;
            curtime=db;
            break;
        }
    }
    regfree(&regex);

    const char *current=str+mch[i].rm_eo+sizeof(char);
    const char *pos=strchr(current,'(');
    if(pos==NULL) return;
    else if((current[0]<'a')||(current[0]>'z')) return;
    strncpy(cur,current,pos-current);
    cur[pos-current]='\0';
}

int main(int argc, char *argv[]) {
    clock_t now=clock();
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
        exec_argv[2]="-ttt";
        for(int i=1;i<argc;i++){
            exec_argv[i+2]=argv[i];
        }
        exec_argv[argc+2]="> /dev/null";
        exec_argv[argc+3]=NULL;
        //exec_argv[argc+2]=NULL;
        execvp("strace",exec_argv);
    }
    else{
        close(pipefd[1]);
        char buffer[4096];
        ssize_t bytesRead;
        while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            /*write(STDOUT_FILENO,buffer,bytesRead);
            printf("\n");
            fflush(stdout);*/
            getname(buffer);
            gettim(buffer);
            memset(buffer,0,sizeof(buffer));
            fflush(stdout);
            clock_t curt=clock();
            if((curt-now)*1000/CLOCKS_PER_SEC>=100){
                merge(1,cnt);
                for(int i=1;i<=5;i++){
                    printf("%s (%d%%)\n",table[i].name,(int)(table[i].time*100.0/sum));
                    fflush(stdout);
                }
                for(int i=1;i<=80;i++) printf("%s",nul);
                fflush(stdout);
                now=curt;
            }
        }
        close(pipefd[0]);
        merge(1,cnt);
        for(int i=1;i<=5;i++){
            printf("%s (%d%%)\n",table[i].name,(int)(table[i].time*100.0/sum));
            fflush(stdout);
        }
        for(int i=1;i<=80;i++) printf("%s",nul);
        fflush(stdout);
    }
    return 0;
}
