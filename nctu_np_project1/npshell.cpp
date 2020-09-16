#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <algorithm>
#include <csignal>

#define MAX_INPUT_LENGTH 15100
#define MAX_TOKEN_LENGTH 10000
#define MAX_PROCESS_IN_LINE 5000
#define MAX_PATH_LENGTH 100

struct Number_pipe {
    int rest_line;
    int p[2]; //pipe
};

bool PipeDead (const Number_pipe &t ) {
    return (t.rest_line==0);
}

// Number_pipe vector
void close_0_rest_line_pipe_out(std::vector<Number_pipe> &num_pipe_vec);
void close_0_rest_line_pipe_in(std::vector<Number_pipe> &num_pipe_vec);

bool checkFileInPATH(char* file);

// handle child process signal, if they finish work
void childHandler(int signo);


int main(void) {

    int childpid, p[2];
    pid_t pid;

    // number pipe
    std::vector <struct Number_pipe> num_pipe_vec;
    struct Number_pipe num_pipe;


    int num_pipe_in_flag=0; // whether number pipe in
    int num_pipe_out_flag=0; // whether |N
    int shock_pipe_out_flag=0; // whether !N
    int write_file_flag = 0; // whether >
    int line_end_with_pipe=0; // deal signal

    char root[20] = "bin:.";

    char shell_prompt[3] = "% ";

    setenv("PATH",root,1);

    while(1) {
        char line[MAX_INPUT_LENGTH]; // command from standard input
        char* token[MAX_TOKEN_LENGTH]; // parsed command
        char* process[MAX_PROCESS_IN_LINE][10]; // process array

        int token_count = 0; // init token count
        int process_count = 0; // init process_count
        int pipe_out_num = STDOUT_FILENO;

        char redirection_file_name[100];
        int redir_file_fd;

        int i = 0; // token index

        line_end_with_pipe = 0;

        // show shell prompt
        write(STDOUT_FILENO, shell_prompt, strlen(shell_prompt));

        // read standard input by line
        fgets(line, MAX_INPUT_LENGTH, stdin);

        // delete 'return' (arcii 10 aka '\n')
        line[strlen(line)-1]=0;


        /* === parsing line into tokens === */
        token[token_count] = strtok(line, " ");
        while( token[token_count]!=NULL ){
            token_count++;
            token[token_count] = strtok(NULL, " ");
        }


        /* === decrease line cout (number pipe) === */
        if( token[i]!=NULL && num_pipe_vec.size()>0 ) {
            for(int i=0; i<num_pipe_vec.size(); i++) {
                num_pipe_vec[i].rest_line--;
                if(num_pipe_vec[i].rest_line==0) {
                    num_pipe_in_flag = 1;
                }
            }
        }

        /* === combine token to process array === */
        while( token[i]!=NULL ) {
            /* === combine tokens to a process === */
            int j=0; // process option count
            while( token[i]!=NULL ){
                if( token[i][0]!='>' && token[i][0]!='|' && token[i][0]!='!' ) {
                    process[process_count][j] = token[i];
                    i++;
                    j++;
                    if (token[i]==NULL) { // if there is no pipe at the end, add NULL
                        process[process_count][j] = NULL;
                    }
                } else {
                    process[process_count][j] = NULL;
                    break;
                }
            } // after loop, the i is now the next token index hasn't process

            // pipe processing
            if( token[i]!=NULL ) {
                if( strcmp(token[i],"|")==0 ) {
                    i++; // next token after '|'
                } else if( strcmp(token[i], ">")==0 ) {
                    write_file_flag = 1;
                    strncpy(redirection_file_name, token[i+1], strlen(token[i+1]));
                    int f = open(redirection_file_name,O_CREAT, 0664);
                    close(f);
                    i+=2; // go throught '>' and 'file name'
                } else if (token[i][0]=='!'){
                    line_end_with_pipe = 1;
                    int num_pipe_line = atoi( strtok(token[i], "!") ); // numberPipe's line number
                    int j=0;
                    for(j=0; j<num_pipe_vec.size(); j++) {
                        if ( num_pipe_line==num_pipe_vec[j].rest_line ) {
                            pipe_out_num = num_pipe_vec[j].p[1];
                            break;
                        }
                    }
                    if (j==num_pipe_vec.size()) {
                        num_pipe.rest_line = num_pipe_line;
                        pipe(num_pipe.p);
                        pipe_out_num = num_pipe.p[1];
                        num_pipe_vec.push_back(num_pipe);
                    }
                    shock_pipe_out_flag = 1;
                    i++;
                } else {
                    line_end_with_pipe = 1;
                    int num_pipe_line = atoi( strtok(token[i], "|") ); // numberPipe's line number
                    int j=0;
                    for(j=0; j<num_pipe_vec.size(); j++) {
                        if ( num_pipe_line==num_pipe_vec[j].rest_line ) {
                            pipe_out_num = num_pipe_vec[j].p[1];
                            break;
                        }
                    }
                    if (j==num_pipe_vec.size()) {
                        num_pipe.rest_line = num_pipe_line;
                        pipe(num_pipe.p);
                        pipe_out_num = num_pipe.p[1];
                        num_pipe_vec.push_back(num_pipe);
                    }
                    num_pipe_out_flag = 1;
                    i++;
                }
            } // the index i is the token after '|'
            process_count++; // count process
        }

        /* === execute process === */
        int prc = 0; // the nth process
        int fd_in = STDIN_FILENO;

        while( prc<process_count ) {
            signal(SIGCHLD, childHandler);
            if( strcmp(process[prc][0],"printenv")==0 ) {
                if(process[prc][1]!=NULL) { // prevent segmentation fault
                    if( getenv(process[prc][1])!=NULL ) { // check if argument is valid to avoid NULL return
                        printf("%s\n", getenv(process[prc][1]));
                        fflush(stdout);
                    }
                }
            } else if( strcmp(process[prc][0],"setenv")==0 ) {
                if( process[prc][1]!=NULL && process[prc][2]!=NULL ) {
                    setenv(process[prc][1],process[prc][2],1);
                }
            } else if( strcmp(process[prc][0],"exit")==0 ) {
                exit(0);
            } else { // deal with PATH process
                if ( write_file_flag==1 && prc==(process_count-1) ) { /* === redirection == */
                    close_0_rest_line_pipe_out( num_pipe_vec ); // if 0 rest line, we need to read p[0] in this process
                    pid=fork();
                    while (pid<0){
                        usleep(1000);
                        pid=fork();
                    }
                    if( pid==0 ) {
                        redir_file_fd = open(redirection_file_name, O_TRUNC | O_WRONLY | O_CREAT, 0664);
                        if( num_pipe_in_flag==0 ) {
                            dup2(fd_in, STDIN_FILENO);
                        } else {
                            for(int i=0; i<num_pipe_vec.size(); i++) {
                                if( num_pipe_vec[i].rest_line==0 ) {
                                    dup2(num_pipe_vec[i].p[0], STDIN_FILENO);
                                    break;
                                }
                            }
                        }
                        dup2(redir_file_fd, STDOUT_FILENO);
                        if (execvp(process[prc][0], process[prc])==-1) {
                            if ( !checkFileInPATH(process[prc][0]) ) {
                                char err_message[100];
                                sprintf(err_message, "Unknown command: [%s].\n", process[prc][0]);
                                write(2,err_message,strlen(err_message));
                                exit(0);
                            }
                        }
                    } else {
                        //wait(NULL);
                        if( process_count!=1 ) close(fd_in); // has normal pipe
                        if( num_pipe_in_flag==1 ) { // close number pipe
                            // close number pipe in
                            close_0_rest_line_pipe_in( num_pipe_vec );
                            // delete number pipe in vector
                            num_pipe_vec.erase(std::remove_if(num_pipe_vec.begin(), num_pipe_vec.end(), PipeDead ), num_pipe_vec.end());
                            num_pipe_in_flag = 0;
                        }
                        write_file_flag = 0;
                    }
                } else if( (shock_pipe_out_flag==1 || num_pipe_out_flag==1) && prc==(process_count-1) ) { /* === |N & !N === */
                    close_0_rest_line_pipe_out( num_pipe_vec ); // if 0 rest line, we need to read p[0] in this process
                    pid=fork();
                    while (pid<0){
                        usleep(1000);
                        pid=fork();
                    }
                    if( pid==0 ) {
                        if( num_pipe_in_flag==0 ) {
                            dup2(fd_in, STDIN_FILENO);
                        } else {
                            for(int i=0; i<num_pipe_vec.size(); i++) {
                                if( num_pipe_vec[i].rest_line==0 ) {
                                    dup2(num_pipe_vec[i].p[0], STDIN_FILENO);
                                    break;
                                }
                            }
                        }
                        if (shock_pipe_out_flag==1){
                            dup2(pipe_out_num, STDERR_FILENO);
                        }
                        dup2(pipe_out_num, STDOUT_FILENO);
                        if (execvp(process[prc][0], process[prc])==-1) {
                            if ( !checkFileInPATH(process[prc][0]) ) {
                                char err_message[100];
                                sprintf(err_message, "Unknown command: [%s].\n", process[prc][0]);
                                write(2,err_message,strlen(err_message));
                                exit(0);
                            }
                        }
                    } else {
                        //wait(NULL);
                        if( process_count!=1 ) close(fd_in); // has normal pipe
                        if( num_pipe_in_flag==1 ) {
                            // close number pipe in
                            close_0_rest_line_pipe_in( num_pipe_vec );
                            // delete number pipe in vector
                            num_pipe_vec.erase(std::remove_if(num_pipe_vec.begin(), num_pipe_vec.end(), PipeDead ), num_pipe_vec.end());
                            num_pipe_in_flag = 0;
                        }
                        num_pipe_out_flag = 0;
                    }
                } else { // normal process and normal pipe
                    if( process_count!=1 ){ // has normal pipe
                        pipe(p);
                    }
                    close_0_rest_line_pipe_out( num_pipe_vec ); // if 0 rest line, we need to read p[0] in this process
                    pid=fork();
                    while (pid<0){
                        usleep(1000);
                        pid=fork();
                    }
                    if( pid==0 ) { // child
                        if ( num_pipe_in_flag==1 ) {
                            for(int i=0; i<num_pipe_vec.size(); i++) {
                                if( num_pipe_vec[i].rest_line==0 ) {
                                    dup2(num_pipe_vec[i].p[0], STDIN_FILENO);
                                    break;
                                }
                            }
                        }

                        if ( process_count!=1 ) { // has number pipe in or normal pipe in
                            if ( num_pipe_in_flag!=1 ) {
                                dup2(fd_in, STDIN_FILENO);
                            }
                            close(p[0]);
                            if( prc!=(process_count-1) ) {
                                dup2(p[1],STDOUT_FILENO); // dup pipe output to standard output
                            }
                        }

                        if (execvp(process[prc][0], process[prc])==-1) {
                            if ( !checkFileInPATH(process[prc][0]) ) {
                                char err_message[100];
                                sprintf(err_message, "Unknown command: [%s].\n", process[prc][0]);
                                write(2,err_message,strlen(err_message));
                                exit(0);
                            }
                        }
                    } else {
                        //wait(NULL);
                        if ( process_count!=1 ){
                            close(p[1]); // close normal pipe output
                            fd_in = p[0];
                            if ( prc==(process_count-1) ){
                                close(fd_in);
                            }
                        }
                        if( num_pipe_in_flag==1 ) {
                            // close 0 rest_line number pipe in
                            close_0_rest_line_pipe_in( num_pipe_vec );
                            // delete 0 rest_line Number_pipe in vector
                            num_pipe_vec.erase(std::remove_if(num_pipe_vec.begin(), num_pipe_vec.end(), PipeDead ), num_pipe_vec.end());
                            num_pipe_in_flag = 0;
                        }
                    }
                }
            } /*else {
                printf("Unknown command: [%s].\n",process[prc][0]);
                fflush(stdout);
            }*/ // bin process
            prc++; // next process
        } // process loop

        if ( !line_end_with_pipe ) {
            //int status;
            while (wait(NULL) > 0) {
            }
        }
    } // infinitive loop
    return 0;
} // main

void close_0_rest_line_pipe_out(std::vector<Number_pipe> &num_pipe_vec) {
    for(int i=0; i<num_pipe_vec.size(); i++) {
        if( num_pipe_vec[i].rest_line==0 ) {
            close(num_pipe_vec[i].p[1]);
        }
    }
}

void close_0_rest_line_pipe_in(std::vector<Number_pipe> &num_pipe_vec) {
    for(int i=0; i<num_pipe_vec.size(); i++) {
        if( num_pipe_vec[i].rest_line==0 ) {
            close(num_pipe_vec[i].p[0]);
        }
    }
}

bool checkFileInPATH(char *file) {
    char PATH[200];
    char real_path[200];
    int i=0;
    char* temp;

    strcpy(PATH,getenv("PATH"));

    temp = strtok(PATH, ":");
    //printf("%s\n", temp);
    while( temp!=NULL ) {
        strcpy(real_path,temp);
        strcat(real_path,"/");
        strcat(real_path,file);
        //printf("path: %s\n",PATH_array[i]);
        if( access(real_path,0)>=0 ) {
            return true;
        }
        //i++;
        //printf("%s\n", temp);
        temp= strtok(NULL, ":");
    }
    return false;
}

void childHandler(int signo) {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {
    }
}
