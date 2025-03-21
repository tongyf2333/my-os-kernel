#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    static char line[4096];
    int cnt=0,status;
    __pid_t pid;

    FILE *fp;
    fp=fopen("/tmp/code.c","a");
    fclose(fp);
    char *args[]={"rm", "/tmp/code.c", NULL};
    pid=fork();
    if(!pid){
        execvp("rm",args);
        exit(0);
    }
    else waitpid(pid,&status,0);
    fp=fopen("/tmp/code.so","a");
    fclose(fp);
    char *args1[]={"rm", "/tmp/code.so", NULL};
    pid=fork();
    if(!pid){
        execvp("rm",args1);
        exit(0);
    }
    else waitpid(pid,&status,0);
    while (1) {
        printf("crepl> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        if(line[0]=='i'&&line[1]=='n'&&line[2]=='t'){
            FILE *fp;
            fp=fopen("/tmp/code.c","a");
            fprintf(fp,"%s\n",line);
            fclose(fp);
            #ifdef __x86_64__
            char *args[]={"gcc", "-shared", "-fPIC", "-o", "/tmp/code.so", "/tmp/code.c", NULL};
            #else
            char *args[]={"gcc", "-m32","-shared", "-fPIC", "-o", "/tmp/code.so", "/tmp/code.c", NULL};
            #endif
            int sta;
            pid=fork();
            if(!pid){
                execvp("gcc",args);
                exit(0);
            }
            else{
                waitpid(pid,&sta,0);
                if(WIFEXITED(sta)&&WEXITSTATUS(sta)!=0){
                    printf("compile error\n");
                    unlink("/tmp/code.c");
                }
            }
        }
        else{
            cnt++;
            char func[4296];
            char name[100];
            sprintf(name,"eval%d",cnt);
            sprintf(func,"int %s(){return %s;}",name,line);
            FILE *fp;
            fp=fopen("/tmp/code.c","a");
            fprintf(fp,"%s\n",func);
            fclose(fp);
            #ifdef __x86_64__
            char *args[]={"gcc", "-Werror","-shared", "-fPIC", "-o", "/tmp/code.so", "/tmp/code.c", NULL};
            #else
            char *args[]={"gcc", "-Werror","-m32","-shared", "-fPIC", "-o", "/tmp/code.so", "/tmp/code.c", NULL};
            #endif
            int statu;
            pid=fork();
            if(!pid){
                execvp("gcc",args);
                exit(0);
            }
            else{
                waitpid(pid,&statu,0);
                if(WIFEXITED(statu)&&WEXITSTATUS(statu)!=0){
                    printf("compile error\n");
                    unlink("/tmp/code.c");
                    continue;
                }
            }
            //printf("QAQ\n");
            void *handle;
            int (*foo)(void);
            char *error;
            handle = dlopen("/tmp/code.so", RTLD_LAZY);
            if (!handle) {
                fprintf(stderr, "%s\n", dlerror());
                return 1;
            }
            dlerror();
            foo=dlsym(handle, name);
            if ((error = dlerror()) != NULL)  {
                fprintf(stderr, "%s\n", error);
                printf("illegal expression\n");
                dlclose(handle);
                //return 1;
            }
            printf("%d\n",foo());
            dlclose(handle);
        }
        //printf("Got %zu chars.\n", strlen(line));
    }
}
