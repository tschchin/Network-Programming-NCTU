#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include "user_2.h"
#include "npshell_2.h"

using namespace std;

#define QLEN 30

int passiveTCP(int SERV_TCP_PORT); // listen (return port)
void err_dump(string s);
void welcome_msg(int fd);
void broadcast_login_msg(int self, char* addr, int port);
void broadcast_logout_msg(int self);
int find_fd_user(int fd);
int find_space_fd();

int user_count=0;
struct User user[30];


int main(int argc, char *argv[]) {
    // server port
    int SERV_TCP_PORT = atoi(argv[1]);

    cout << "RWS SERVER open !" << endl;

    // clear all env param
    //clearenv();
    setenv("PATH","bin:.",1);

    for(int i=0; i<QLEN; ++i) {
        user[i].valid = 0;
        user[i].id = i+1;
    }

    struct sockaddr_in fsin;
    int msock;
    fd_set rfds; // read file descriptor set
    fd_set afds; // active file descriptor set

    socklen_t alen; // address length
    int fd,nfds;

    msock = passiveTCP(SERV_TCP_PORT);
    cout << "listening port[" << SERV_TCP_PORT << "]" << endl;

    nfds = getdtablesize(); // get file descriptor table size
    FD_ZERO(&afds);
    FD_SET(msock, &afds);

    int fd_in = dup(STDIN_FILENO);
    int fd_out = dup(STDOUT_FILENO);
    int fd_err = dup(STDERR_FILENO);

    while(1) {
        memcpy(&rfds, &afds, sizeof(rfds));

        if(select(nfds, &rfds, (fd_set*)0, (fd_set*)0, (struct timeval *)0)<0){
            if (errno == EINTR)
    			continue;
  			else
    			err_dump("select error\n");
        }

        if(FD_ISSET(msock, &rfds)) {
            int ssock;
            alen = sizeof(fsin);
            ssock = accept(msock, (struct sockaddr *)&fsin, &alen);
            if(ssock<0) {
                err_dump("accept error\n");
            } else {
                char* addr = inet_ntoa(fsin.sin_addr);
                int port = fsin.sin_port;
                int space_ind; //space index

                // find the space id
                for(int i=0; i<QLEN; ++i) {
                    if(user[i].valid==0) {
                        user[i].valid = 1;
                        space_ind = i;
                        break;
                    }
                }

                cout << "client[" << space_ind+1 << "] connected from" << addr << "/" << port << endl;;

                user[space_ind].sock = ssock;
                strcpy(user[space_ind].addr,addr);
                strcpy(user[space_ind].env,"bin:.");
                strcpy(user[space_ind].name,"(no name)");
                user[space_ind].port = port;
                user[space_ind].batch_flag=0;
                user[space_ind].ignore=0;
                //user[space_ind].name_pipe_flag=0;

                welcome_msg(ssock);
                broadcast_login_msg(space_ind,addr,port);
                write(ssock, "% ", strlen("% "));
            }
            FD_SET(ssock,&afds);
        }

        for(fd=0; fd<nfds; ++fd) {
            if(fd!=msock && FD_ISSET(fd, &rfds)) {
                int cc;
                char line[15000];
                int user_ind;
                user_ind = find_fd_user(fd);

                dup2(fd,STDIN_FILENO);
                dup2(fd,STDOUT_FILENO);
                dup2(fd,STDERR_FILENO);

                /*cc = read(fd, line, 15000);
                int l = strlen(line);
                cout << cc << l << endl;*/

                if(npshell(user_ind, user[user_ind].num_pipe_vec)==0) {
                    dup2(fd_in,STDIN_FILENO);
                    dup2(fd_out,STDOUT_FILENO);
                    dup2(fd_err,STDERR_FILENO);
                    cout << "client[" << user_ind+1 << "] disconnect." << endl;
                    close(fd);
                    FD_CLR(fd, &afds);
                }  else {
                    /*for(int i=0; i<user[user_ind].broadcast_msg.size(); ++i) {
                        write(fd,user[user_ind].broadcast_msg[i].c_str(),user[user_ind].broadcast_msg[i].length());
                    }
                    user[user_ind].broadcast_msg.clear();*/
                    //npshell(user_ind, user[user_ind].num_pipe_vec)
                    write(fd, "% ", strlen("% "));
                    dup2(fd_in,STDIN_FILENO);
                    dup2(fd_out,STDOUT_FILENO);
                    dup2(fd_err,STDERR_FILENO);
                }
            }
        }
    }



    return 0;
}

int passiveTCP(int SERV_TCP_PORT) {
    int sockfd;
    socklen_t clilen;
    struct sockaddr_in serv_addr;

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

    return sockfd;
}

void err_dump(string s) {
    cerr << s << endl;
	//fprintf(stderr,"%s\n",s);
	//exit(1);
}

int find_fd_user(int fd) {
    for(int i=0; i<QLEN; ++i) {
        if(user[i].valid==1 && user[i].sock==fd) {
            return i;
        }
    }
    return -1;
}

void welcome_msg(int fd) {
    char wel_msg[] = "****************************************\n"
                    "** Welcome to the information server. **\n"
                    "****************************************\n";
    write(fd,wel_msg,strlen(wel_msg));
}

void broadcast_login_msg(int self, char* addr, int port) {
    char login_msg[200];
    //sprintf(login_msg,"*** User '(no name)' entered from %s/%d. ***\n",addr,port);
    sprintf(login_msg,"*** User '(no name)' entered from CGILAB/511. ***\n"); //addr/port
    //broadcast_msg.push_back(login_msg);
    for(int i=0; i<QLEN; ++i) {
        if(user[i].valid==1) {
            write(user[i].sock,login_msg,strlen(login_msg));
            /*if( i==self) {
                write(user[i].sock,login_msg,strlen(login_msg));
            } else {
                user[i].broadcast_msg.push_back(login_msg);
            }*/
        }
    }
}
