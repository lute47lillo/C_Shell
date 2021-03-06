#include <stdlib.h>
#include <stdio.h>                                                          
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>
#include "mycs334shell.h"


/**
 Reads the full command line given by the user character by character.
 **/
char* read_command(){
    int BUFFER_SIZE = 1024;
    int index = 0;
    char* read_buffer = malloc(sizeof(read_buffer) * BUFFER_SIZE);
    int character;
   
    //check if buffer was correctly allocated 
    if(read_buffer ==  NULL){
        fprintf(stderr, "Error creating the buffer\n");
        exit(1);
    }
    
    //Read char by char from the stdin and put it in the buffer until end of line.
    while(1){
        character = getchar();
        if(character == EOF || character == '\n'){
            read_buffer[index] = '\0';
            return read_buffer;
        }else{
            read_buffer[index] = character;
        }
        index++;
    }
}

/**
Gets the command line read previously and splits it into the actual command to execute and its arguments.
**/

char** parse_commands(char* command){
    int BUFFER_SIZE = 1024;
    char** arguments_buffer = malloc(sizeof(char*) * BUFFER_SIZE);
    char* word;
    int index = 0;
    
    //Splits the string by words.
    word = strtok(command, " ");

    //While word is not null, keeps splitting and storing it for argument
    while(word){
        arguments_buffer[index] = word;
        index++;
        word = strtok(NULL, " ");
    }
    
    arguments_buffer[index] = NULL;
    return arguments_buffer;
}


int runTime(char** args){

    //Check if the run time symbol is given
    char* time_symbol = "@";
    int c = 0;

    int timelimit = 0;
    //Shift the commands one to the left, deleting the "@" symbol and get time limit
    if(strncmp(args[c], time_symbol, 1) == 0){
        timelimit = atoi(args[1]);
        //printf("%d", timelimit);
        for(int i = 0; args[i]!=NULL;i++){
            args[i] = args [i+2];
            //printf("%s\n", args[i]);
        }
    }
    return timelimit;
}
/**
 Will execute if a pipe is found. Creates a pipe connections between 2 child processes.
 **/
void pipe_execution(char** temp_args, char** second_args, int isBackground, int time_limit){
    struct rlimit r_limit;
    pid_t pid1;
    pid_t pid2;
    int rc;
    
    //Create the pipe and check for error
    int pipefd[2];
    if(pipe(pipefd) == -1){
        perror("pipe");
        exit(1);
    }
    
    //Forks first child
    if((pid1 = fork()) < -1){
        perror("fork failed");
        exit(1);
    }
    
    if(pid1 == 0){
        
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        //Set the time limit usage of the cpu
        if(time_limit>0){
            getrlimit(RLIMIT_CPU, &r_limit);
            r_limit.rlim_cur = time_limit;
            setrlimit(RLIMIT_CPU, &r_limit);
        }
        
        if(execvp(temp_args[0], temp_args) < 0){
            perror("exec");
            exit(0);
        }
        
    }else{
        
        if(isBackground){
            waitpid(pid1, &rc, 0);
        }
        
        //Other child
        if((pid2 = fork()) < -1){
            perror("fork failed");
            exit(1);
        }
        
        if(pid2 == 0){
           
        
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);

            if(execvp(second_args[0], second_args) < 0){
                perror("exec");
                exit(0);
            }
            
        }else{
            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
        }
    }
}

/**
 Checks if a pipe is requested by the user and parses the arguments for the pipe execution
 **/
int check_pipe(char** args, int time_limit){
    struct rlimit r_limit;
    char* piping = "|";
    int BUFFER_SIZE = 1024;
    char** temp_args = malloc(sizeof(char*) * BUFFER_SIZE);
    char** second_args = malloc(sizeof(char*) * BUFFER_SIZE);
    int isBackground = 0;
    
    //Check if character "|" exists
    for(int i = 0; args[i]!=NULL; i++){
   
        if(strncmp(args[i], piping, 1) == 0){
            //Check for background processes with pipes
            int b = 0;
            while(args[b]){
                b++;
            }
            if(strncmp(args[b-1], "&", 1) == 0){
                isBackground = 1;
                args[b-1] = NULL;
            }
           
            //Parse the pipe process
            for(int j = 0; j < i;j++){
                temp_args[j] = args[j];
                
            }
            temp_args[i]=NULL;

            int count;
            for(int p = 0; args[i]!=NULL; p++){
                second_args[p] = args[i+1];
                i++;
                count = i;
            }
            second_args[count] = NULL;
            
            //Execute the pipe
            pipe_execution(temp_args, second_args, isBackground, time_limit);

            free(temp_args);
            free(second_args);
          
            return 1;
        }
    }
    return 0;
}

/**
 Executes the commands by creating child processes that will handle the execution.
 **/
int execute_command(char** args, int time_limit){
   
   //Processes variables 
    pid_t pid;
    pid_t wpid;
    pid_t watchdog_pid;
    int rc;
    int isBackground = 0;
    
    //Time Limit
    struct rlimit r_limit;

    //Exits our shell
    if (strcmp(args[0], "exit") == 0) {
        return 0;
    }
    
    //Checks if there is a background job to be handle and no pipes were involved
    int b = 0;
    while(args[b]){
        b++;
    }
    if(strncmp(args[b-1], "&", 1) == 0){
        isBackground = 1;
        args[b-1] = NULL;
    }
    
    //If the process of creating a child fails
    if((pid = fork()) < 0 ){
        perror("fork failed");
        exit(1);
    }
       
    //Child process
    if(pid == 0){
        char file_to_write[1024];
        char file_to_read[1024];
    
        int isWriting = 0;
        int isReading = 0;
        
        char* redirection_output = ">";
        char* redirection_input = "<";
        
        //Check for redirection.
        for(int i = 0; args[i]!=NULL;i++){
            
            if(strcmp(args[i], redirection_output) == 0){
                args[i] = NULL;
                strcpy(file_to_write, args[i+1]);
                isWriting = 1;
                break;
            }
          
            if(strcmp(args[i], redirection_input) == 0){
                args[i] = NULL;
                strcpy(file_to_read, args[i+1]);
                isReading = 1;
                break;
            }
        }
   
        //Execution of the redirection commands
        if(isWriting){
            int fdWrite;
            
            if((fdWrite = creat(file_to_write, 0644)) == -1){
                perror("open");
                exit(0);
            }
            dup2(fdWrite, STDOUT_FILENO);
            close(fdWrite);
        }
        
        if(isReading){
            int fdRead;
            if((fdRead = open(file_to_read, O_RDONLY, 0)) == -1){
                perror("open input failed");
                exit(0);
            }
            dup2(fdRead, STDIN_FILENO);
            close(fdRead);
        }
        
        //Set the time limit usage of the cpu
        if(time_limit>0){
            getrlimit(RLIMIT_CPU, &r_limit);
            r_limit.rlim_cur = time_limit;
            setrlimit(RLIMIT_CPU, &r_limit);
        }

        //Index 0 -> the command, the rest -> arguments.
        if(execvp(args[0], args) < 0){
            perror("exec");
            exit(0);
        }
    
    }else{     //Parent process
        
        if(isBackground){
            wpid = waitpid(pid, &rc, 0); //Wait for the child process to finish if is background
        }else{
            waitpid(pid, &rc, WUNTRACED); //Check for non-background processes
        }
    }
    
    return 1;
}


int main(){
    
    char* command;  //Hold commands(strings) by the user
    char** arguments;  //Arguments to be call with the command
    int succes = 1;  //for exit, exit and returns 0
    int pipe_flag;  //Checks if pipe is used
    int time_limit = 0; //Set to 0 by default -> it will run until it is done.

    //Call the loop of execution of the shell.
    do {
        //Flush the previous content in the buffer.
        fflush(stdout);
        printf("CS334:) ");
        
        //Read full command
        command = read_command();
        if(strlen(command)<1){
            printf("Hi");
        }
        //Get arguments one by one
        arguments = parse_commands(command);

        //Check if time limit is set
        time_limit = runTime(arguments);

        //Check if pipe instruction is done
        pipe_flag = check_pipe(arguments, time_limit);
        
        if(pipe_flag == 0){  //If not, execute normal
            succes = execute_command(arguments, time_limit);
        }else if(pipe_flag == 1){
            succes = 1;
        }
        
        //frees the commands and the arguments.
        free(command);
        free(arguments);
    } while (succes);
    
    return 0;
}
