#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h> // fork
#include <signal.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#define SHMKEY_USERS ((key_t)7899)
//#define SEMKEY_NAME_PIPE_RECORD ((key_t)7891)
//#define SEMKEY2 ((key_t)7892)
#define PERMS 0666


#define SHMKEY ((key_t) 10000)
#define SEMKEY1 ((key_t) 7891)
#define SEMKEY2 ((key_t) 7892)



#include "npshell_3.h"
#include "user_3.h"
#include "semaphore.h"

#define QLEN 30 /* how many people can connect to server */
// #define SERV_TCP_PORT 7000 /* server tcp port */

using namespace std;

void err_dump(const char* s);
void welcome_msg();
void broadcast_login_msg(char* addr, int port);
static void real_broadcast_msg(int signal_number);
static void server_exit(int signo);
static void client_exit(int signo);

int user_count=0;
User *user;
int shm_user_id, shm_name_pipe_id, clisem,servsem,clisem1;
int sockfd, newsockfd, childpid;

int main(int argc, char *argv[]) {
    char login_msg[] = "*** User '(no name)' entered from CGILAB/511. ***\n";
    //sprintf(login_msg,"*** User '(no name)' entered from %s/%d. ***\n",addr,port);
    //sprintf(login_msg,"*** User '(no name)' entered from CGILAB/511. ***\n"); //addr/port

    signal(SIGUSR1,real_broadcast_msg);
    signal(SIGCHLD,client_exit); // client exit
    signal(SIGINT,server_exit); // server exit
    //signal(17,real_broadcast_msg);
    //printf("%d",SIGUSR1);
    //fflush(stdout);

    //*********

    // create share memory
    if((shm_user_id=shmget(SHMKEY_USERS, sizeof(User)*30, IPC_CREAT | PERMS))<0) {
        err_dump("server: can't get user share memory\n");
    }
    // attach to share mrmory
    user = (User *)shmat(shm_user_id,(char*)0,0);

    // create semaphore
    if ( (clisem = sem_create(SEMKEY1,1)) < 0) {
        err_dump("clisem error\n");
    }
    /*if ( (clisem1 = sem_create(SEMKEY2,1)) < 0) {
        err_dump("clisem error\n");
    }*/
    //*********

    int SERV_TCP_PORT = atoi(argv[1]);

    //int sockfd, newsockfd, childpid;
    socklen_t clilen;
    struct sockaddr_in cli_addr, serv_addr;

    /*
    * Open a TCP socket (an internet stream socket)
    */
    if((sockfd=socket(AF_INET,SOCK_STREAM,0))<0) {
        err_dump("server: can't open stream socket");
    }

    /*
    * Bind local address
    */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERV_TCP_PORT);
    if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) {
        err_dump("server: can't bind local address");
    }

    /*
    * listen
    */
    listen(sockfd, QLEN);

    /*
    * initial user
    */
    for(int i=0; i<QLEN; ++i) {
        user[i].valid = 0;
        user[i].id = i+1;
    }

    /*
    * accept client to connect
    */
    while(1) {
        memset(&cli_addr, 0, sizeof(cli_addr));
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd,(struct sockaddr *)&cli_addr, &clilen);

        // initial user value
        char* addr = inet_ntoa(cli_addr.sin_addr);
        int port = cli_addr.sin_port;
        int space_ind; //user index

        sem_wait(clisem);
        // find the space id
        for(int i=0; i<QLEN; ++i) {
            if(user[i].valid==0) {
                user[i].valid = 1;
                space_ind = i;
                break;
            }
        }

        printf("client[%d]\tis connected from %s/%d\tsocket[%d].\n",space_ind+1,addr,port,newsockfd);

        user[space_ind].sock = newsockfd;
        strcpy(user[space_ind].addr,addr);
        //strcpy(user[space_ind].env,"bin:.");
        strcpy(user[space_ind].name,"(no name)");
        user[space_ind].port = port;
        user[space_ind].batch_flag=0;
        user[space_ind].ignore=0;
        user[space_ind].receive=0;
        user[space_ind].msg_count=0;
        sem_signal(clisem);
        //user[space_ind].broadcast_msg_count=0;

        if(newsockfd<0) {
            err_dump("server: accept error");
        }
        if((childpid=fork())<0) {
            err_dump("server: fork error");
        } else if(childpid==0) { /* child process */
            /* close original socket */
            close(sockfd);

            sem_wait(clisem);
            user[space_ind].pid = getpid();
            sem_signal(clisem);


            /* redirect stdin stdout stderr */
            dup2(newsockfd, STDIN_FILENO);
            dup2(newsockfd, STDOUT_FILENO);
            dup2(newsockfd, STDERR_FILENO);

            /* process request */
            welcome_msg();
            //sem_wait(clisem);
            broadcast_login_msg(user[space_ind].addr, user[space_ind].port);

            for(int i=0; i<QLEN; ++i) {
                if(user[i].valid==1 && i!=space_ind) {
                    close(user[i].sock);
                }
            }

            if (npshell(space_ind,user)==0) {
                close(newsockfd);
                if(shmdt(user)<0) {
                    err_dump("server: can't detach shared memory.\n");
                }
            }
            exit(0);
        }
        //close(newsockfd); /* parent process */
        //i++;
    }

    return 0;
}

void err_dump(const char* s) {
	fprintf(stderr,"%s\n",s);
	//exit(1);
}

void welcome_msg() {
    printf("****************************************\n** Welcome to the information server. **\n****************************************\n");
    fflush(stdout);
}

void broadcast_login_msg(char* addr, int port) {
    char login_msg[200];
    //sprintf(login_msg,"*** User '(no name)' entered from %s/%d. ***\n",addr,port);
    sprintf(login_msg,"*** User '(no name)' entered from CGILAB/511. ***\n"); //addr/port
    //broadcast_msg.push_back(login_msg);
    for(int i=0; i<QLEN; ++i) {
        if(user[i].valid==1) {
            write(user[i].sock,login_msg,strlen(login_msg));
            //sprintf(user[i].broadcast_msg[user[i].broadcast_msg_count],login_msg,200);
            //user[i].broadcast_msg_count++;
        }
    }
}

void real_broadcast_msg(int signal_number) {
    //printf("here!!\n");
    //fflush(stdout);
    //sem_wait(clisem);
    int pid = getpid();
    for(int i=0; i<QLEN; ++i) {
        if(user[i].valid==1 && user[i].pid==pid) {
            //write(user[i].sock, user[i].broadcast_msg,strlen(user[i].broadcast_msg));
            for(int j=0; j<user[i].msg_count; j++) {
                printf("%s",user[i].broadcast_msg[j]);
                fflush(stdout);
            }
            //write(sockfd, user[i].broadcast_msg,strlen(user[i].broadcast_msg));
            //user[i].receive=0;
            user[i].msg_count=0;
            break;
       }
       //
   }
   //sem_signal(clisem);
}
static void server_exit(int signo) {
    if(shmdt(user)<0) {
        err_dump("server: can't detach shared memory.\n");
    }
    if(shmctl(shm_user_id, IPC_RMID, (struct shmid_ds *) 0)<0) {
        err_dump("share memory close erro\n");
    }
    printf("SEVER QUIT!\n");
    close(sockfd);
    sem_close(clisem);
    //sem_close(clisem1);
    exit(0);
}
static void client_exit(int signo) {
    pid_t pid;
    pid = wait(NULL);
    int i;
    for(i=0; i<QLEN; ++i) {
        if(user[i].pid==pid) {
            close(user[i].sock);
            break;
        }
    }
    printf("client[%d]\tis disconnect.\n",user[i].id);
}
