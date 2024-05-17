#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

double tim[4096];
int tot;

void parseNumbersInAngleBrackets(const char *str) {
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
        if(db!=0.0) tim[++tot]=db;
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
            parseNumbersInAngleBrackets(buffer);
            memset(buffer,0,sizeof(buffer));
            //printf("\n");
            fflush(stdout);
        }
        close(pipefd[0]);
        printf("%d\n",tot);
        for(int i=1;i<=tot;i++) printf("%lf\n",tim[i]);
    }
    return 0;
}
