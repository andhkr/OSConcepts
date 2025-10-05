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
    // char* iterator = command;
    
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

    for(int i = 0;i<MAXCMD;++i){
        if(args[i] != NULL){
        }else break;
    }
    execvp(args[0],args);
}

// PIPE COMMUNICATION

// close all unsed file descriptor
// it is always benificial to release unsed descriptor
// because kernel have limited resources and all are in use
// demanding process will be blocked till release and it also help
// process in detecting EOF on pipes or files as mentioned below
void closeAllUnsedPipeFd(int pipelist[][2],int pipecount){
    for(int i = 0;i<pipecount;++i){
        close(pipelist[i][0]);
        close(pipelist[i][1]);
    }
};

void pipeStart(int cid,int pipelist[][2],char** argslist,int pipecount){
    //setting output to pipe write end
    dup2(pipelist[cid][1],STDOUT_FILENO);
    // close all unsed file descriptor
    closeAllUnsedPipeFd(pipelist,pipecount);
    // execute command
    commandexecute(argslist[cid]);
}

void pipeMid(int cid,int pipelist[][2],char** argslist,int pipecount){
    // setting input to pipe of parent

    dup2(pipelist[cid-1][0],STDIN_FILENO);
    // setting output to next process
    dup2(pipelist[cid][1],STDOUT_FILENO);

    // close all unsed file descriptor
    closeAllUnsedPipeFd(pipelist,pipecount);
    //execute command
    commandexecute(argslist[cid]);
}

void pipeEnd(int cid,int pipelist[][2],char** argslist,int pipecount){
    // read from previous process
    dup2(pipelist[cid-1][0],STDIN_FILENO);
    close(pipelist[cid-1][1]);

    // close all unsed file descriptor
    closeAllUnsedPipeFd(pipelist,pipecount);
    //execute command
    commandexecute(argslist[cid]);
}

typedef struct PipeComDetails{
    pid_t* cpids;
    int pipecount;
}PipeComDetails;

PipeComDetails pipecommunication(char* command){
    // number of pipes required
    int pipecount = 0;
    char* iterator = command;
    while(*iterator){
        if(*iterator == '|') pipecount++;
        iterator++;
    }

    // pipe creation for each pair
    int pipelist[pipecount][2];

    for(int i = 0;i<pipecount;++i){
        if(pipe(pipelist[i]) == -1){
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // command for each process
    char* argslist[pipecount+1];
    char* handle = strtok(command,"|");
    int i = 0;
    while(handle){
        argslist[i++] = handle;
        handle = strtok(NULL,"|");
    }

    pid_t* cpids = (pid_t*) malloc(sizeof(pid_t)*(pipecount+1));
    int childId = 0;

    // launch first process
    cpids[childId] = fork();
    if(cpids[childId] == -1){
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if(cpids[childId] == 0){ // child process;
        pipeStart(childId,pipelist,argslist,pipecount);
    }

    // in between process
    for(childId = 1; childId < pipecount; ++childId){
        // launch  process
        cpids[childId] = fork();
        if(cpids[childId] == -1){
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if(cpids[childId] == 0){ // child process;
            pipeMid(childId,pipelist,argslist,pipecount);
        }
    }

    // launch last process
    cpids[childId] = fork();
    if(cpids[childId] == -1){
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if(cpids[childId] == 0){ // child process;
        pipeEnd(childId,pipelist,argslist,pipecount);
    }

    // close all file descriptor by parent(my shell )
    // because due to parent open file discriptor pipe 
    // will not released back to os and last child process
    // waiting for input will be blocked 
    // but in reality all producer have produced and finished
    // e.g ls | grep expr
    // after producing list ls will exited , grep produce the result 
    // but since pipe end is also referenced by shell it will not released
    // and grep will be blocked and eventually shell will be blocked
    for(int i = 0;i<pipecount;++i){
        close(pipelist[i][0]);
        close(pipelist[i][1]);
    }

    // return the child process ids so that parent can wait for each one to finish
    PipeComDetails pDetails = {.cpids = cpids,.pipecount=pipecount};
    return pDetails;
}

int main(){
    // The program can support command of max lenght MAXCMDLEN 
    char command[MAXCMDLEN];
    char prevcommand[MAXCMDLEN];

    // shell Interface: OsConcepts$
    while(canRun){
        printf("OsConcepts:~$ ");
        fflush(stdout);

        if(fgets(command,MAXCMDLEN,stdin) != NULL){

            command[strcspn(command,"\n")] = '\0';

            if(strcmp(command,"\\q")==0 || strcmp(command,"exit")==0){
                break;
            }

            if(strcmp(command,"!!")==0){
                strcpy(command,prevcommand);
            }

            // history feature;
            strcpy(prevcommand,command);

            // background feature (currently for only one &)
            char* havebgsymb = strchr(command,'&');
            if(havebgsymb != NULL) *havebgsymb = '\0';

            //pipe feature
            char* havepipe = strchr(command,'|');
            if(havepipe != NULL){
                PipeComDetails pDetails = pipecommunication(command);

                if(havebgsymb != NULL) continue; // no wait

                for(int i = 0;i<=pDetails.pipecount;++i){

                    int rstatus = 0;
                    int r_pid = waitpid(pDetails.cpids[i],&rstatus,0);
                    if(rstatus == EXIT_FAILURE){
                        fprintf(stderr,"Input Given May be Not given as per rule\n");
                        exit(EXIT_FAILURE);
                    }
                }
            }else{ // without pipe normal commands
            
                pid_t cpid = fork();

                if(cpid == -1){
                    perror("fork");
                    exit(EXIT_FAILURE);
                }

                if( cpid == 0){ // child process
                    commandexecute(command);
                }
                else{ // parent process
                    if(havebgsymb == NULL){
                        int status;
                        pid_t r_wait = wait(&status);
                        if(status == EXIT_FAILURE){
                            fprintf(stderr,"Wrong commands or buffer overflow\n");
                        }
                    }
                }   
            }
        }else{
            fprintf(stderr,"Error in fgets\n");
            break;
        }
    }

}
