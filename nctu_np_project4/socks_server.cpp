#include <array>
#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/buffer.hpp>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <string>
#include <fstream>

using namespace std;
using namespace boost::asio;

io_service ioservice;

class HttpSession : public enable_shared_from_this<HttpSession> {
  private:
    enum { max_length = 1024 };
    ip::tcp::socket _in_socket;
    string _ip;
    string _port;
    char* _reply;
    boost::asio::ip::tcp::resolver::query _query;
    boost::asio::ip::tcp::resolver _resolver;
    boost::asio::ip::tcp::socket _out_socket;
    array<char, max_length> _read_data;
    array<char, max_length> _write_data;
    int flag1;
    int flag2;
  public:
    HttpSession(ip::tcp::socket socket, string ip, string port, char* reply) : _in_socket(move(socket)),
        _ip(ip),
        _port(port),
        _reply(reply),
        _query(_ip,_port),
        _resolver(ioservice),
        _out_socket(ioservice) {
            flag1 = 0;
            flag2 = 0;
        }
    
    void start() {
        do_connect();
    }
  private:
    void do_connect() {
        auto self(shared_from_this());
        _resolver.async_resolve(_query, [this,self](const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator it) {         
            if (!ec) {            
                _out_socket.async_connect(*it, [this,self](const boost::system::error_code &ec) {                     
                    if (!ec) {
                        socks_reply();
                    } else {
                        cout << "CONNECT ERROR: " << ec.message() << endl;
                    }
                }); 
            } else {                   
                cout << "RESOLVE ERROR: " << ec.message() << endl;
            }
        });  
    };
    void socks_reply() {
        auto self(shared_from_this());
        _in_socket.async_send(
            buffer(_reply, 8),
            [self,this](boost::system::error_code ec, std::size_t ) {
                if(!ec) {
                    in_do_read();
                    out_do_read();
                } else {
                    // std::cerr << "Accept error: " << ec.message() << std::endl;
                }
            });
    }
    void in_do_read() {
        auto self(shared_from_this());
        _in_socket.async_read_some(
            buffer(_write_data, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    in_do_write(length);
                } else {
                    flag1 = 1;
                    _in_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
                    _out_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
                    if(flag2==1) {
                        _in_socket.close();
                        _out_socket.close();
                    }
                    //cout << "c READ1 ERROR: " << ec.message() << endl;
                };
            });
    }
    
    void in_do_write(std::size_t r_length) {
        auto self(shared_from_this());
        _out_socket.async_send(
            buffer(_write_data, r_length),
                [self,this,r_length](boost::system::error_code ec, std::size_t s_length) {
                    if(!ec) {
                        if(r_length!=s_length) {
                            size_t len = r_length - s_length;
                            in_do_write(len);
                        } else {
                            in_do_read();
                        }
                        //out_do_read();
                    } else {
                        // std::cerr << "Accept error: " << ec.message() << std::endl;
                    }
            });
    }

    void out_do_read() {
        auto self(shared_from_this());
        _out_socket.async_read_some(
            buffer(_read_data, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    out_do_write(length);
                } else {
                    flag2 = 1;
                    _out_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
                    _in_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
                    if(flag1==1) {
                        _in_socket.close();
                        _out_socket.close();
                    }
                    //cout << "c READ2 ERROR: " << ec.message() << endl;
                };
            });
    }

    void out_do_write(std::size_t r_length) {
        auto self(shared_from_this());
        _in_socket.async_send(
            buffer(_read_data, r_length),
                [self,this,r_length](boost::system::error_code ec, std::size_t s_length) {
                    if(!ec) {
                        if(r_length!=s_length) {
                            size_t len = r_length - s_length;
                            out_do_write(len);
                        } else {
                            out_do_read();
                        }
                        //out_do_read();
                    } else {
                        // std::cerr << "Accept error: " << ec.message() << std::endl;
                    }
            });
    }
};

/*class Bindtalk : public enable_shared_from_this<Bindtalk> {

};*/

class BindSession : public enable_shared_from_this<BindSession> {
  private:
    enum { max_length = 1024 };
    array<char, max_length> _read_data;
    array<char, max_length> _write_data;
    ip::tcp::acceptor _acceptor;
    ip::tcp::endpoint _endPoint;
    ip::tcp::socket _socket;
    ip::tcp::socket _rsocket;
    string _ip;
    string _port;
    char* _reply;
    int flag1;
    int flag2;
    //int len1;
    //int len2;
  public:
    BindSession(ip::tcp::socket socket, string ip, string port, char* reply)
      : _acceptor(ioservice),
        _endPoint(ip::tcp::endpoint(ip::tcp::v4(), 0)),
        _socket(move(socket)),
        _rsocket(ioservice),
        _ip(ip),
        _port(port),
        _reply(reply) {
        flag1 = 0;
        flag2 = 0;
        //len1 = 0;
        //len2 = 0;
    }
    void start() { do_connect(); }
  private:
    void do_connect() {
        auto self(shared_from_this());
        unsigned short port(0);

        _acceptor.open(_endPoint.protocol());
        _acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
        _acceptor.bind(_endPoint);
        _acceptor.listen();  
        
        port =  _acceptor.local_endpoint().port();

        _reply[2] = port/256;
        _reply[3] = port%256;
        
        do_reply();
    }

    void do_reply() {
        auto self(shared_from_this());
        _socket.async_send(
            buffer(_reply, 8),
            [self,this](boost::system::error_code ec, std::size_t ) {
                if(!ec) {
                    do_accept();
                } else {
                    std::cerr << "send reply error: " << ec.message() << std::endl;
                }
            });
    }

    void do_accept() {
        auto self(shared_from_this());
        _acceptor.async_accept(_rsocket, [self,this](boost::system::error_code ec) {
                if(!ec) {
                    _socket.async_send(
                    buffer(_reply, 8),
                    [self,this](boost::system::error_code ec, std::size_t ) {
                        if(!ec){
                            out_do_read();
                            in_do_read();
                            //do_read_write_all();
                        }
                    });
                } else {
                    // std::cerr << "Accept error: " << ec.message() << std::endl;
                }
            });
    }
    void do_read_write_all() {
        int cli_fd = _socket.native_handle();
        int ser_fd = _rsocket.native_handle();

    }
    void in_do_read() {
        auto self(shared_from_this());
        _socket.async_read_some(
            buffer(_write_data, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    in_do_write(length);
                } else {
                    flag1 = 1;
                    _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
                    _rsocket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
                    if(flag2==1) {
                        _socket.close();
                        _rsocket.close();
                    }
                    // cout << "b READ1 ERROR: " << ec.message() << endl;
                };
            });
    }

    void in_do_write(std::size_t r_length) {
        auto self(shared_from_this());
        _rsocket.async_send(
            buffer(_write_data, r_length),
                [self,this,r_length](boost::system::error_code ec, std::size_t s_length) {
                    if(!ec) {
                        if(r_length!=s_length) {
                            size_t len = r_length - s_length;
                            in_do_write(len);
                        } else {
                            in_do_read();
                        }
                        //out_do_read();
                    } else {
                        // std::cerr << "Accept error: " << ec.message() << std::endl;
                    }
            });
    }

    void out_do_read() {
        auto self(shared_from_this());
        _rsocket.async_read_some(
            buffer(_read_data, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    out_do_write(length);
                } else {
                    flag2 = 1;
                    _rsocket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
                    _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
                    if(flag1==1) {
                        _socket.close();
                        _rsocket.close();
                    }
                    // cout << "b READ2 ERROR: " << ec.message() << endl;
                };
            });
    }

    void out_do_write(std::size_t r_length) {
        auto self(shared_from_this());
        _socket.async_send(
            buffer(_read_data, r_length),
            [self,this,r_length](boost::system::error_code ec, std::size_t s_length) {
                if(!ec) {
                    if(r_length!=s_length) {
                        size_t len = r_length - s_length;
                        out_do_write(len);
                    } else {
                        out_do_read();
                    }
                } else {
                    // std::cerr << "Accept error: " << ec.message() << std::endl;
                }
            });
    }
};

class SockSession : public enable_shared_from_this<SockSession> {
 private:
  enum { max_length = 1024 };
  char ACCEPT = 90;
  char REJECT = 91;
  char _reply[8];
  ip::tcp::socket _socket;
  array<char, max_length> _request;
  map<string, string> _info; 
  map<string, string> _sock_request;

 public:
  SockSession(ip::tcp::socket socket) : _socket(move(socket)) {
        _sock_request["SOURCE_IP"] = _socket.remote_endpoint().address().to_string();
        _sock_request["SOURCE_PORT"] = to_string(_socket.remote_endpoint().port());
        for(int i=0; i<8; i++) {
            _reply[i] = 0;
        }
  }

  void start() { do_read(); }

private:
    void do_read() {
        auto self(shared_from_this());
        _socket.async_read_some(
        buffer(_request, max_length),
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                /*for(auto x: _request) {
                    cout << int(x) << " ";
                }
                cout << endl;*/
                parse_request();
                if(firewall(_sock_request["CD"])==1){
                    show_host_sock_info(_sock_request["CD"],1);
                    if(_sock_request["CD"]=="c")
                        doConnectSession();
                    else if(_sock_request["CD"]=="b")
                        doBindSession();
                } else {
                    show_host_sock_info(_sock_request["CD"],0);
                    rejectRequest();
                };
            };
        });
    }
    void doBindSession() {
        _reply[0] = 0;
        _reply[1] = ACCEPT;
        for(int i=2; i<8; i++) {
            _reply[i]=0;
        }
        make_shared<BindSession>(move(_socket), _sock_request["DST_IP"], _sock_request["DST_PORT"], move(_reply))->start();
    }

    void doConnectSession() {
        if(_sock_request["DST_IP"]!="0.0.0.0") {
            _reply[0] = 0;
            _reply[1] = ACCEPT;
            for(int i=2; i<8; i++) {
                _reply[i]=_request[i];
            }
            make_shared<HttpSession>(move(_socket), _sock_request["DST_IP"], _sock_request["DST_PORT"], move(_reply))->start();
        }
    }

    void rejectRequest() {
        auto self(shared_from_this());
        _reply[0] = 0;
        _reply[1] = 91;
        for(int i=2; i<8; i++) {
            _reply[i]=_request[i];
        }
        _socket.async_send(
            buffer(_reply, 8),
            [self](boost::system::error_code ec, std::size_t ) {
                if(!ec) {
                } else {
                    // std::cerr << "Accept error: " << ec.message() << std::endl;
                }
        });
    }

    int firewall(string CD) {
        fstream socks_conf("socks.conf");
        string line;
        if(socks_conf) {
            while(getline(socks_conf,line)) {
                vector<string> socks_vec;
                int pos = line.find("#");
                if(pos!=string::npos) {
                    line = line.substr(0,pos);
                }
                istringstream iss(line);
                for(std::string line; iss >> line; ){
                    socks_vec.push_back(line);
                }
                if(socks_vec[0]=="permit" && socks_vec[1]==CD) {
                    int pos = socks_vec[2].find("*");
                    string pattern;
                    if(pos!=string::npos)
                        pattern = socks_vec[2].substr(0,pos);
                    else
                        pattern = socks_vec[2];
                    string ip = _sock_request["DST_IP"].substr(0,pattern.length());
                    if(ip==pattern) {
                        return 1;
                    }
                }
            } 
        }
        return -1;
    }
    void show_host_sock_info(string CD,int result) {
        cout << "<S_IP>:\t" << _sock_request["SOURCE_IP"] << endl;
        cout << "<S_PORT>:\t" << _sock_request["SOURCE_PORT"] << endl;
        cout << "<D_IP>:\t" << _sock_request["DST_IP"] << endl;
        cout << "<D_PORT>:\t" << _sock_request["DST_PORT"] << endl;
        if(_sock_request["CD"]=="c")
            cout << "<Command>:\t" << "CONNECT" << endl;
        else if(_sock_request["CD"]=="b")
            cout << "<Command>:\t" << "BIND" << endl;
        if(result==1)
            cout << "<Reply>:\t" << "Accept" << endl;
        else
            cout << "<Reply>:\t" << "Reject" << endl;
        cout << endl;
    }
    void parse_request() {
        _sock_request["VN"] = to_string(bytetoint(_request[0]));
        if(bytetoint(_request[1])==1)
            _sock_request["CD"] = "c";
        else if(bytetoint(_request[1])==2)
            _sock_request["CD"] = "b";//to_string(bytetoint(_request[1]));
        _sock_request["DST_PORT"] = to_string(bytetoint(_request[2])*256 + bytetoint(_request[3]));
        _sock_request["DST_IP"] = to_string(bytetoint(_request[4]))+"."+to_string(bytetoint(_request[5]))+"."+to_string(bytetoint(_request[6]))+"."+to_string(bytetoint(_request[7]));
    }
    int bytetoint(char c) {
        int a=0;
        int r=1;
        for(int i=0; i<8; i++) {
            a += (c&1)*r;
            c = c>>1;
            r = r*2;
        }
        return a;
    }
};

class SockServer {
    private:
        enum { max_length = 1024 };
        ip::tcp::acceptor _acceptor;
        ip::tcp::socket _socket;
        boost::asio::signal_set _signal;
        //array<char, max_length> data;
    public:
        SockServer(short port)
            : _acceptor(ioservice, ip::tcp::endpoint(ip::tcp::v4(), port)),
            _socket(ioservice),
            _signal(ioservice, SIGCHLD) {
            wait_for_signal();
            do_accept();
        }

    private:
        void wait_for_signal() {
            _signal.async_wait(
                [this](boost::system::error_code /*ec*/, int /*signo*/) {
                    if (_acceptor.is_open()) {
                        int status = 0;
                        while (waitpid(-1, &status, WNOHANG) > 0) {}
                        wait_for_signal();
                    }
            });
        }
        void do_accept() {
            _acceptor.async_accept(_socket, [this](boost::system::error_code ec) {
                if(!ec) {
                    ioservice.notify_fork(boost::asio::io_service::fork_prepare);
                    if (fork() == 0) {
                        ioservice.notify_fork(boost::asio::io_service::fork_child);
                        _acceptor.close();
                        _signal.cancel();

                        make_shared<SockSession>(move(_socket))->start();
                    } else {
                        ioservice.notify_fork(boost::asio::io_context::fork_parent);
                        _socket.close();
                        do_accept();
                    }
                } else {
                    // std::cerr << "Accept error: " << ec.message() << std::endl;
                    do_accept();
                }
            });
        }
};

int main(int argc, char* const argv[]) {
  if (argc != 2) {
    std::cerr << "Usage:" << argv[0] << " [port]" << endl;
    return 1;
  }

  try {
    short port = atoi(argv[1]);
    SockServer server(port);
    ioservice.run();
  } catch (exception& e) {
    cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}