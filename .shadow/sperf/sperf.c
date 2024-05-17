#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>

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
        int cnt=0;
        while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            //write(STDOUT_FILENO, buffer, bytesRead);
            cnt++;
        }
        close(pipefd[0]);
        printf("%d\n",cnt);
    }
    return 0;
}
