#ifndef SHELL
#define SHELL

char* read_command();
char** parse_commands(char* command);
void pipe_execution(char** temp_args, char** second_args, int isBackground, int time_limit);
int check_pipe(char** args, int time_limit);
int execute_command(char** args, int time_limit);
int runTime(char** command);

#endif