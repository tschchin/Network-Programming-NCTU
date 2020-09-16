#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h> // fork

#include "npshell.h"

#define QLEN 30 /* how many people can connect to server */
// #define SERV_TCP_PORT 7000 /* server tcp port */

void err_dump(const char* s);
void test(int sockfd);
void welcome_msg();

int main(int argc, char *argv[]) {

    int SERV_TCP_PORT = atoi(argv[1]);

    int sockfd, newsockfd, childpid;
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
    * accept client to connect
    */
    while(1) {
        memset(&cli_addr, 0, sizeof(cli_addr));
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd,(struct sockaddr *)&cli_addr, &clilen);
        if(newsockfd<0) {
            err_dump("server: accept error");
        }
        if((childpid=fork())<0) {
            err_dump("server: fork error");
        } else if(childpid==0) { /* child process */
            /* close original socket */
            close(sockfd);

            /* redirect stdin stdout stderr */
            dup2(newsockfd, STDIN_FILENO);
            dup2(newsockfd, STDOUT_FILENO);
            dup2(newsockfd, STDERR_FILENO);

            /* process request */
            //welcome_msg();
            npshell();

            exit(0);
        }
        close(newsockfd); /* parent process */
    }

    return 0;
}

void err_dump(const char* s) {
	fprintf(stderr,"%s\n",s);
	//exit(1);
}

void test(int sockfd) {
    char tt[101];
    /*dup2(sockfd, STDIN_FILENO);
    dup2(sockfd, STDOUT_FILENO);
    dup2(sockfd, STDERR_FILENO);*/
    while(scanf(" %s",tt)) {
        write(STDOUT_FILENO,tt,strlen(tt));
        //fflush(stdout);
    }
        //write(sockfd,tt,strlen(tt));
}

void welcome_msg() {
    printf("***************************************\n** Welcome to the information server **\n***************************************\n");
    fflush(stdout);
}
