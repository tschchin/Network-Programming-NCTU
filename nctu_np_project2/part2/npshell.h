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
#define QLEN 30

#define MAX_INPUT_LENGTH 15100
#define MAX_TOKEN_LENGTH 10000
#define MAX_PROCESS_IN_LINE 5000
#define MAX_PATH_LENGTH 100

bool PipeDead (const Number_pipe &t ) {
    return (t.rest_line==0);
}

// Number_pipe vector
void close_0_rest_line_pipe_out(std::vector<Number_pipe> &num_pipe_vec);
void close_0_rest_line_pipe_in(std::vector<Number_pipe> &num_pipe_vec);

bool checkFileInPATH(char* file);

// handle child process signal, if they finish work
void childHandler(int signo);



int check_name_pipe_exist(int sender, int receiver);
int check_name_exist(char *name);
int check_id_exist(int id);
void broadcast_msg(const char* msg);

int name_pipe_record[QLEN+1][QLEN+1] = {0}; // by id
extern struct User user[QLEN];


int npshell(int user_index, std::vector <Number_pipe> &num_pipe_vec) {
    setenv("PATH",user[user_index].env,1);

    int sock = user[user_index].sock;
    int cc;

    int childpid, p[2];
    pid_t pid;

    // number pipe
    //std::vector <struct Number_pipe> *num_pipe_vec = user_vec.num_pipe_vec;


    struct Number_pipe num_pipe;


    int num_pipe_in_flag = 0; // whether number pipe in
    int num_pipe_out_flag = 0; // whether |N
    int shock_pipe_out_flag = 0; // whether !N
    int write_file_flag = 0; // whether >
    int line_end_with_pipe = 0; // deal signal
    int name_pipe_out_flag = 0;
    int name_pipe_in_flag = 0;

    int batch_flag = user[user_index].batch_flag;
    int ignore = user[user_index].ignore;


    char batch_file[100];
    char batch_command[10][100];
    int batch_count=0;
    int batch_command_index=0;

    //while(1) {
    char line[MAX_INPUT_LENGTH]; // command from standard input
    char* token[MAX_TOKEN_LENGTH]; // parsed command
    char* process[MAX_PROCESS_IN_LINE][10]; // process array

    int token_count = 0; // init token count
    int process_count = 0; // init process_count
    int pipe_out_num = STDOUT_FILENO;

    char redirection_file_name[100] = {0};
    int redir_file_fd;

    char name_pipe_out_file[100];
    char name_pipe_in_file[100];
    int name_pipe_sender_id;



    int i = 0; // token index
    line[0] = 0; // init line

    line_end_with_pipe = 0;


    if( batch_flag==0 ) { // normal read from keyboard
        cc = read(sock, line, MAX_INPUT_LENGTH);

        // To Socket
        // strlen(line)-1 == (arcii 13)
        // strlen(line)-2 == (arcii 10)
        for(int i=0; i<MAX_INPUT_LENGTH; i++) {
            if(line[i]<20) {
                line[i] = 0;
                break;
            }
        }
        //write(sock,line,strlen(line));
    } else if ( batch_flag==1 ){ // read cmd from file
        if ( batch_count!=0 ) {
            if ( batch_command_index<batch_count ) {
                strncpy(line, batch_command[ batch_command_index ], strlen(batch_command[ batch_command_index ])-1);
                line[strlen(batch_command[ batch_command_index ])-1]=0;
                //printf("%s\n", line);
                batch_command_index++;
            } else {
                batch_count=0;
                batch_command_index=0;
                batch_flag=0;
                ignore=1;
            }
        }
    }

    // store command for name pipe (since strtok will destroy line)
    char command[MAX_INPUT_LENGTH];
    sprintf(command, line, strlen(line));

    char err_msg[300]={0};
    int name_pipe_out_id;


    if(strlen(line)==0){ // space input
        //ignore = 1;
        return cc;
    }


    // *** parsing line into tokens ***
    token[token_count] = strtok(line, " ");
    while( token[token_count]!=NULL ){
        token_count++;
        token[token_count] = strtok(NULL, " ");
    }

    // *** decrease line cout (number pipe) ***
    if( token[i]!=NULL && num_pipe_vec.size()>0 ) {
        for(int i=0; i<num_pipe_vec.size(); i++) {
            num_pipe_vec[i].rest_line--;
            if(num_pipe_vec[i].rest_line==0) {
                num_pipe_in_flag = 1;
            }
        }
    }

    int tell_yell_flag = 0;
    if( strcmp(token[0],"tell")==0 || strcmp(token[0],"yell")==0 ) {
        tell_yell_flag = 1;
    }

    // *** combine token to process array ***
    while( token[i]!=NULL ) {
        // *** combine tokens to a process ***
        int j=0; // process option count
        if ( tell_yell_flag==0 ) {
            while( token[i]!=NULL ) {
                if ( token[i][0]=='<' ) {
                    name_pipe_sender_id =  atoi( strtok(token[i], "<") );
                    if(check_id_exist(name_pipe_sender_id)==0) { // user do not exist
                        printf("*** Error: user #%d does not exist yet. ***\n",name_pipe_sender_id);
                        fflush(stdout);
                        //ignore=1;
                        return cc;
                    } else {
                        if(check_name_pipe_exist(name_pipe_sender_id,user_index+1)==1) { // name pipe exist
                            name_pipe_in_flag = 1;
                            //sprintf(name_pipe_in_file,"user_pipe/%d-%d",name_pipe_sender_id,user_index+1);
                            name_pipe_record[name_pipe_sender_id][user_index+1] = 0;
                            sprintf(err_msg,"*** %s (#%d) just received from %s (#%d) by '%s' ***\n",user[user_index].name,user[user_index].id,user[name_pipe_sender_id-1].name,name_pipe_sender_id,command);
                            broadcast_msg(err_msg);
                        } else {
                            printf("*** Error: the pipe #%d->#%d does not exist yet. ***\n",name_pipe_sender_id,user_index+1);
                            //ignore=1;
                            return cc;
                        }
                    }
                    i++;
                    if (token[i]==NULL) { // if there is no pipe at the end, add NULL
                        if(j==0) {
                            process_count--;
                        } else {
                            process[process_count][j] = NULL;
                        }
                    }
                } else if( token[i][0]!='>' && token[i][0]!='|' && token[i][0]!='!' ) {
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
                    //write(sock,,strlen(redirection_file_name));
                    int f = open(redirection_file_name,O_CREAT, 0664);
                    //write(sock,f,sizeof(f));
                    //printf("\n1. %d\n",f);
                    fflush(stdout);
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
                } else if ( token[i][0]=='|' && strlen(token[i])>1 ) {
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
                        //printf("%d %d\n",num_pipe.p[0],num_pipe.p[1]);
                        pipe_out_num = num_pipe.p[1];
                        num_pipe_vec.push_back(num_pipe);
                    }
                    num_pipe_out_flag = 1;
                    i++;
                } else if ( token[i][0]=='>' && strlen(token[i])>1 ) { // name pipe out
                    // get receiver id
                    name_pipe_out_id = atoi(strtok(token[i], ">"));

                    if(check_id_exist(name_pipe_out_id)==0) { // user do not exist
                        printf("*** Error: user #%d does not exist yet. ***\n",name_pipe_out_id);
                        fflush(stdout);
                        //ignore=1;
                        return cc;
                    } else {
                        if (check_name_pipe_exist(user_index+1, name_pipe_out_id)==0) {  // do not have name pipe yet
                            //sprintf(name_pipe_out_file,"user_pipe/%d-%d",user_index+1,name_pipe_out_id);
                            //int f = open(name_pipe_out_file,O_CREAT, 0664);
                            //close(f);
                            pipe(user[name_pipe_out_id-1].name_pipe[user_index]);
                            name_pipe_record[user_index+1][name_pipe_out_id]=1;
                            name_pipe_out_flag = 1;
                        } else {
                            printf("*** Error: the pipe #%d->#%d already exists. ***\n",user_index+1,name_pipe_out_id);
                            //ignore = 1;
                            return cc;
                        }
                    }
                    i++;
                }
            } // the index i is the token after '|'
        } else { // tell_yell command
            while( token[i]!=NULL ) {
                process[0][j] = token[i];
                //printf("%s\n",token[j]);
                i++;
                j++;
                if (token[i]==NULL) { // if there is no pipe at the end, add NULL
                    process[0][j] = NULL;
                }
            }
        }
        process_count++; // count process
    }


    // *** execute process ***
    if( ignore==0 ) {
        int prc = 0; // the nth process
        int fd_in = STDIN_FILENO;

        while( prc<process_count ) {
            signal(SIGCHLD, childHandler);
            if( strcmp(process[prc][0],"printenv")==0 ) {
                if(process[prc][1]!=NULL) { // prevent segmentation fault
                    if( getenv(process[prc][1])!=NULL ) { // check if argument is valid to avoid NULL return
                        char env[200];
                        sprintf(env,"%s\n", getenv(process[prc][1]));
                        write(user[user_index].sock,env,strlen(env));
                        //fflush(stdout);
                    }
                }
            } else if( strcmp(process[prc][0],"setenv")==0 ) {
                if( process[prc][1]!=NULL && process[prc][2]!=NULL ) {
                    setenv(process[prc][1],process[prc][2],1);
                    sprintf(user[user_index].env,"%s",process[prc][2]);
                }
            } else if( strcmp(process[prc][0],"exit")==0 ) {
                // broadcast logout msg
                sprintf(err_msg,"*** User '%s' left. ***\n",user[user_index].name);
                broadcast_msg(err_msg);

                // clear all name pipe record
                for(int i=1; i<QLEN+1; i++) {
                    for(int j=1; j<QLEN+1; j++) {
                        if( i==user_index+1 || j==user_index+1) {
                            name_pipe_record[i][j] = 0;
                        }
                    }
                }
                // clear user
                user[user_index].valid = 0;

                return 0;
            } else if ( strcmp(process[prc][0],"batch")==0 ) {
                batch_flag=1;
                strcpy(batch_file, process[prc][1]);
                FILE * pFile;
                pFile = fopen (batch_file , "r");
                while( fgets(batch_command[batch_count] , 20 , pFile)!=NULL ) {
                    batch_count++;
                }
                fclose(pFile);
                batch_command_index = 0;
            } else if ( strcmp(process[prc][0],"who")==0 ) {
                printf("<ID>\t<nickname>\t<IP/port> \t<indicate me>\n");
                for(int i=0; i<QLEN; ++i) {
                    if( user[i].valid==1 ) {
                        if(user[i].sock==sock ) {
                            //printf("%d\t%s\t%s/%d\t<-me\n",i+1,user[i].name,user[i].addr,user[i].port);  addr/port
                            printf("%d\t%s\tCGILAB/511\t<-me\n",i+1,user[i].name);
                        } else{
                            //printf("%d\t%s\t%s/%d\t\n",i+1,user[i].name,user[i].addr,user[i].port); addr/port
                            printf("%d\t%s\tCGILAB/511\t\n",i+1,user[i].name);
                        }
                    }
                }
            } else if( strcmp(process[prc][0],"tell")==0 ) {
                int id = atoi(process[prc][1]);
                char msg[200];
                if(check_id_exist(id)==1) {
                    char tell_msg[200]={0};
                    int i=2;
                    while(process[prc][i]!=NULL) {
                        strcat(tell_msg,process[prc][i]);
                        strcat(tell_msg," ");
                        i++;
                    }
                    sprintf(msg,"*** %s told you ***: %s\n",user[user_index].name, tell_msg);
                    write(user[id-1].sock,msg,strlen(msg));
                } else {
                    printf("*** Error: user #%d does not exist yet. ***\n",id);
                }
            } else if( strcmp(process[prc][0],"yell")==0 ) {
                char msg[200];
                char yell_msg[200]={0};
                int i=1;
                while(process[prc][i]!=NULL) {
                    strcat(yell_msg,process[prc][i]);
                    strcat(yell_msg," ");
                    i++;
                }
                sprintf(msg,"*** %s yelled ***: %s\n",user[user_index].name, yell_msg);
                broadcast_msg(msg);
            } else if( strcmp(process[prc][0],"name")==0 ) {
                if(process[prc][1]==NULL) {
                    strcpy(user[user_index].name, "(no name)");
                } else {
                    if(check_name_exist(process[prc][1])==1) {
                        printf(" *** User '%s' already exists. ***\n",process[prc][1]);
                    } else {
                        strcpy(user[user_index].name, process[prc][1]);
                        char msg[200];
                        //sprintf(msg,"*** User from %s/%d is named '%s'. ***\n",user[user_index].addr,user[user_index].port,user[user_index].name);addr/port
                        sprintf(msg,"*** User from CGILAB/511 is named '%s'. ***\n",user[user_index].name);
                        broadcast_msg(msg);
                    }
                }

            } else { // narmal process
                if ( write_file_flag==1 && prc==(process_count-1) ) { // === redirection ==
                    close_0_rest_line_pipe_out( num_pipe_vec ); // if 0 rest line, we need to read p[0] in this process
                    if( name_pipe_in_flag==1 ) {
                        close(user[user_index].name_pipe[name_pipe_sender_id-1][1]);
                    }
                    pid=fork();
                    while (pid<0){
                        usleep(1000);
                        pid=fork();
                    }
                    if( pid==0 ) {
                        redir_file_fd = open(redirection_file_name, O_TRUNC | O_WRONLY | O_CREAT, 0664);
                        //printf("\n2. %d\n",redir_file_fd);
                        fflush(stdout);
                        if( num_pipe_in_flag==1 ) {
                            for(int i=0; i<num_pipe_vec.size(); i++) {
                                if( num_pipe_vec[i].rest_line==0 ) {
                                    dup2(num_pipe_vec[i].p[0], STDIN_FILENO);
                                    break;
                                }
                            }
                        } else if( name_pipe_in_flag==1 ) {
                            //int file_fd = open(name_pipe_in_file, O_RDONLY, 0664);
                            //dup2(file_fd,STDIN_FILENO);
                            dup2(user[user_index].name_pipe[name_pipe_sender_id-1][0],STDIN_FILENO);
                        } else {
                            dup2(fd_in, STDIN_FILENO);
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
                        if( name_pipe_in_flag==1 ) {
                            close(user[user_index].name_pipe[name_pipe_sender_id-1][0]);
                            name_pipe_in_flag = 0;
                        }
                        write_file_flag = 0;
                    }
                } else if( (shock_pipe_out_flag==1 || num_pipe_out_flag==1) && prc==(process_count-1) ) { // === |N & !N ===
                    close_0_rest_line_pipe_out( num_pipe_vec ); // if 0 rest line, we need to read p[0] in this process
                    if( name_pipe_in_flag==1 ) {
                        close(user[user_index].name_pipe[name_pipe_sender_id-1][1]);
                    }
                    pid=fork();
                    while (pid<0){
                        usleep(1000);
                        pid=fork();
                    }
                    if( pid==0 ) {
                        if( num_pipe_in_flag==1 ) {
                            for(int i=0; i<num_pipe_vec.size(); i++) {
                                if( num_pipe_vec[i].rest_line==0 ) {
                                    dup2(num_pipe_vec[i].p[0], STDIN_FILENO);
                                    break;
                                }
                            }
                        } else if( name_pipe_in_flag==1 ) {
                            //int file_fd = open(name_pipe_in_file, O_RDONLY, 0664);
                            //dup2(file_fd,STDIN_FILENO);
                            dup2(user[user_index].name_pipe[name_pipe_sender_id-1][0],STDIN_FILENO);
                        } else {
                            dup2(fd_in, STDIN_FILENO);
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
                        if( name_pipe_in_flag==1 ) {
                            close(user[user_index].name_pipe[name_pipe_sender_id-1][0]);
                            name_pipe_in_flag = 0;
                        }
                        num_pipe_out_flag = 0;
                    }
                } else if ( name_pipe_out_flag==1 && prc==(process_count-1) ) {
                    close_0_rest_line_pipe_out( num_pipe_vec );
                    if( name_pipe_in_flag==1 ) {
                        close(user[user_index].name_pipe[name_pipe_sender_id-1][1]);
                    }
                    pid=fork();
                    while (pid<0){
                        usleep(1000);
                        pid=fork();
                    }
                    if( pid==0 ) {
                        char msg[200];
                        sprintf(msg,"*** %s (#%d) just piped '%s' to %s (#%d) ***\n",user[user_index].name,user[user_index].id,command,user[name_pipe_out_id-1].name,user[name_pipe_out_id-1].id);
                        broadcast_msg(msg);

                        int file_fd = open(name_pipe_out_file, O_TRUNC | O_WRONLY | O_CREAT, 0664);

                        if( num_pipe_in_flag==1 ) {
                            for(int i=0; i<num_pipe_vec.size(); i++) {
                                if( num_pipe_vec[i].rest_line==0 ) {
                                    dup2(num_pipe_vec[i].p[0], STDIN_FILENO);
                                    break;
                                }
                            }
                        } else if( name_pipe_in_flag==1 ) {
                            //int file_fd = open(name_pipe_in_file, O_RDONLY, 0664);
                            //dup2(file_fd,STDIN_FILENO);
                            dup2(user[user_index].name_pipe[name_pipe_sender_id-1][0],STDIN_FILENO);
                        } else {
                            dup2(fd_in, STDIN_FILENO);
                        }

                        dup2(user[name_pipe_out_id-1].name_pipe[user_index][1], STDOUT_FILENO);
                        if (execvp(process[prc][0], process[prc])==-1) {
                            if ( !checkFileInPATH(process[prc][0]) ) {
                                char err_message[100];
                                sprintf(err_message, "Unknown command: [%s].\n", process[prc][0]);
                                write(2,err_message,strlen(err_message));
                                exit(0);
                            }
                        }
                    } else {
                        if( process_count!=1 ) close(fd_in); // has normal pipe
                        if( num_pipe_in_flag==1 ) { // close number pipe
                            // close number pipe in
                            close_0_rest_line_pipe_in( num_pipe_vec );
                            // delete number pipe in vector
                            num_pipe_vec.erase(std::remove_if(num_pipe_vec.begin(), num_pipe_vec.end(), PipeDead ), num_pipe_vec.end());
                            num_pipe_in_flag = 0;
                        }
                        if( name_pipe_in_flag==1 ) {
                            close(user[user_index].name_pipe[name_pipe_sender_id-1][0]);
                            name_pipe_in_flag = 0;
                        }
                        name_pipe_out_flag==1;
                    }
                } else { // normal process and normal pipe
                    if( process_count!=1 ){ // has normal pipe
                        pipe(p);
                    }
                    close_0_rest_line_pipe_out( num_pipe_vec ); // if 0 rest line, we need to read p[0] in this process
                    if( name_pipe_in_flag==1 ) {
                        close(user[user_index].name_pipe[name_pipe_sender_id-1][1]);
                    }
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
                        } else if( name_pipe_in_flag==1 ) {
                            //int file_fd = open(name_pipe_in_file, O_RDONLY, 0664);
                            //dup2(file_fd,STDIN_FILENO);
                            dup2(user[user_index].name_pipe[name_pipe_sender_id-1][0],STDIN_FILENO);
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
                        if( name_pipe_in_flag==1 ) {
                            close(user[user_index].name_pipe[name_pipe_sender_id-1][0]);
                            name_pipe_in_flag = 0;
                        }
                    }
                }
            }
            prc++; // next process
        } // process loop

        if ( !line_end_with_pipe ) {
            //int status;
            while (wait(NULL) > 0) {
            }
        }
    } else {
        ignore=1;
    } // to prevent batch command for execute last command twice
    //} // infinitive loop
    return cc;
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
int check_id_exist(int id) {
    for(int i=0; i<QLEN; ++i) {
        if(user[i].valid==1) {
            if( i==(id-1) ) {
                return 1;
            }
        }
    }
    return 0;
}

int check_name_pipe_exist(int sender, int receiver) {
    //printf("+++++\n");
    if( name_pipe_record[sender][receiver]==1 ) {
        //name_pipe_record[sender][receiver]=0;
        return 1;
    }
    return 0;
}

int check_name_exist(char *name) {
    for(int i=0; i<QLEN; ++i) {
        if(user[i].valid==1) {
            if( strcmp(user[i].name,name)==0 ) {
                return 1;
            }
        }
    }
    return 0;
}

void broadcast_msg(const char* msg) {
    for(int i=0; i<QLEN; ++i) {
        if(user[i].valid==1) {
            write(user[i].sock,msg,strlen(msg));
        }
    }
}
