#ifndef USER_H
#define USER_H

typedef struct {
    int valid;
    int id;
    int pid;
    int sock;
    //char env[200];
    char addr[20];
    char name[30];
    int port;
    int batch_flag;
    int ignore;
    int name_pipe_record[31]; // by index
    char broadcast_msg[10][1500];
    int msg_count;
    int receive;
    //int broadcast_msg_count;
} User;

#endif




//extern std::vector <struct User> user_vec;
