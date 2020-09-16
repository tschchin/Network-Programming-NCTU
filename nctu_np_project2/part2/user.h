#include <vector>
#include <string>

using namespace std;


struct Number_pipe {
    int rest_line;
    int p[2]; //pipe
};


struct User {
    int valid;
    int id;
    int sock;
    char env[200];
    char addr[15];
    char name[30];
    int port;
    int batch_flag;
    int ignore;
    vector <Number_pipe> num_pipe_vec;
    int name_pipe[30][2]; // by index
    //`int name_pipe_flag[30];
};



//extern std::vector <struct User> user_vec;
