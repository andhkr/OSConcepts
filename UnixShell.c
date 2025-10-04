/**
 * Implementing a C program to serve as a shell interface
 * that accepts user commands and then executes each command 
 * in a separate process.
 * It supports:
 * ** Redirecting Input and Output (<,>)
 * ** Communication via a Pipe (only two process : p1 | p2)
 * ** Mini History(!!) feature (OsConcepts:~$ !!)
 * ** background process running (command &)
 */

#include<stdio.h>
#include<stdbool.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<string.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<ctype.h>

#define MAXCMDLEN 128
#define MAXCMD 10

bool canRun = true;

bool commandexecute(char* command){

    size_t posn =  strcspn(command,"<>|");
    int cmdcnt = 0;
    char* ptr = strndup(command,posn);
    char* cp = ptr;


    char* args[MAXCMD+1];
    int i = 0;
    args[i++] = strtok(ptr," \t\n");

    while(args[i-1]){
        args[i++]= strtok(NULL," \t\n");
        if(i>MAXCMD){
            fprintf(stderr,"Too Many Arguments\n");
            exit(EXIT_FAILURE);
        }
    }

    // input redirection
    char* input = strchr(command,'<');

    if(input != NULL){
        ++input;//skikpping the '<'
        while(isspace(*input))++input;

        size_t pos = strcspn(input," >\t\n");
        char* inputfile = strndup(input,pos);

        if(strlen(inputfile) == 0){
            fprintf(stderr,"Error input File is not Given");
            exit(EXIT_FAILURE);
        }

        int fd = open(inputfile,O_RDONLY);
        dup2(fd,STDIN_FILENO);
    }

    // output redirection
    char* output = strchr(command,'>');
    bool isOutputRedirected = false;
    if( output!= NULL){
        isOutputRedirected = true;
        ++output;//skipping '>'
        while(isspace(*output))++output;

        size_t pos = strcspn(output," \t\n");
        char* outputfile = strndup(output,pos);

        if(strlen(outputfile) == 0){
            fprintf(stderr,"Error output File is not Given");
            exit(EXIT_FAILURE);
        }

        int fd = open(outputfile,O_WRONLY|O_CREAT|O_TRUNC,0644);
        dup2(fd,STDOUT_FILENO);
    }

    char* pipeposn = strchr(command,'|');

    if( pipeposn != NULL){ // pipe communication
        if(isOutputRedirected){
            fprintf(stderr,"Error: you want piped communication but already output redirection given\n");
            exit(EXIT_FAILURE);
        }

        int pipefd[2];

        if(pipe(pipefd) == -1){
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        
        pid_t cpid = fork();
        if(cpid == -1){
            perror("fork");
            exit(EXIT_FAILURE);
        }
        
        if(cpid == 0){ // child process
            dup2(pipefd[0],STDIN_FILENO);
            close(pipefd[1]);
            ++pipeposn;
            while(isspace(*pipeposn))++pipeposn;
            commandexecute(strdup(pipeposn));
        }else{ // parent process
            dup2(pipefd[1],STDOUT_FILENO);
            close(pipefd[0]);
        }
        
    }
    execvp(args[0],args);
}

int main(){
    // The program can support command of max lenght MAXCMDLEN 
    char command[MAXCMDLEN];

    // shell Interface: OsConcepts$
    while(canRun){
        printf("OsConcepts:~$ ");
        fflush(stdout);

        if(fgets(command,MAXCMDLEN,stdin) != NULL){
            command[strcspn(command,"\n")] = '\0';

            if(strcmp(command,"\\q")==0 || strcmp(command,"exit")==0){
                break;
            }

            pid_t cpid = fork();

            if(cpid == -1){
                perror("fork");
                exit(EXIT_FAILURE);
            }

            if( cpid == 0){ // child process
                commandexecute(command);
            }
            else{ // parent process
                if(strchr(command,'&') == NULL){
                    int status;
                    pid_t r_wait = wait(&status);
                    if(status == EXIT_FAILURE){
                        fprintf(stderr,"Wrong commands or buffer overflow\n");
                    }
                }
            }
        }else{
            fprintf(stderr,"Error in fgets\n");
            break;
        }

    }

}
