#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>
#include <string>
#include <ctime> 
#include <boost/asio/signal_set.hpp>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>
#include <memory>
#include <utility>
#include <array>

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

io_service ioservice;
short port;

class HttpServer : public enable_shared_from_this<HttpServer> {
    private:
        enum { max_length = 1024 };
        ip::tcp::socket _socket;
        array<char, max_length> _data;
        map<string,string> _env;
    public:
        HttpServer(ip::tcp::socket socket) : _socket(move(socket)) {
            _env["REMOTE_ADDR"] = _socket.remote_endpoint().address().to_string();
            _env["REMOTE_PORT"] = to_string(_socket.remote_endpoint().port());
            _env["SERVER_ADDR"] = _socket.local_endpoint().address().to_string();
            _env["SERVER_PORT"] = to_string(_socket.local_endpoint().port());
        }
        void start() { do_read(); }
    private:
        void do_read() {
            auto self(shared_from_this());
            _socket.async_read_some(
                buffer(_data, max_length),
                [this, self](boost::system::error_code ec, std::size_t length) {
                    if(!ec) {
                        string req("");
                        for(auto x:_data)
                            req+=x;
                        //cerr << req;
                        parseHttpRequest(req);
                        setEnv();
                        connect_to_url();
                        //exit(0);
                    }
                });
        }

        void parseHttpRequest(string req) {
            // get http request header
            string line_delimiter = "\n";
            string req_header = req.substr(0,req.find(line_delimiter));
            
            // get http host
            req.erase(0,req.find(line_delimiter)+line_delimiter.length());
            string host_line = req.substr(0,req.find(line_delimiter));
            _env["HTTP_HOST"] = host_line.substr(req.find(" ")+1,host_line.length());
            _env["HTTP_HOST"] = _env["HTTP_HOST"].substr(0,_env["HTTP_HOST"].find(":"));
            
            // parse header
            string header_delimiter = " ";
            size_t pos = 0;

            // Request Method
            pos = req_header.find(header_delimiter);
            _env["REQUEST_METHOD"] = req_header.substr(0, pos);
            req_header.erase(0, pos + header_delimiter.length());
            //cout << _env["REQUEST_METHOD"] << endl;

            // Request URI & Query String
            pos = req_header.find(header_delimiter);
            string uri_and_query = req_header.substr(0, pos);
            _env["REQUEST_URI"] = uri_and_query;
            //string temp = uri_and_query.substr(0, uri_and_query.find("?"));
            if (uri_and_query.find("?") != string::npos)
                _env["QUERY_STRING"] = uri_and_query.substr(uri_and_query.find("?")+1,uri_and_query.length());
            else
                _env["QUERY_STRING"] = "";
            req_header.erase(0, pos + header_delimiter.length());

            // Server Protocol
            _env["SERVER_PROTOCOL"] = req_header;
            //cout << _env["SERVER_PROTOCOL"] << endl;
        }

        void setEnv() {
            setenv("REQUEST_METHOD",_env["REQUEST_METHOD"].c_str(),1);
            setenv("REQUEST_URI",_env["REQUEST_URI"].c_str(),1);
            setenv("QUERY_STRING",_env["QUERY_STRING"].c_str(),1);
            setenv("SERVER_PROTOCOL",_env["SERVER_PROTOCOL"].c_str(),1);
            setenv("HTTP_HOST",_env["HTTP_HOST"].c_str(),1);
            setenv("SERVER_ADDR",_env["SERVER_ADDR"].c_str(),1);
            setenv("SERVER_PORT",_env["SERVER_PORT"].c_str(),1);
            setenv("REMOTE_ADDR",_env["REMOTE_ADDR"].c_str(),1);
            setenv("REMOTE_PORT",_env["REMOTE_PORT"].c_str(),1);
        }

        void connect_to_url() {
            auto self(shared_from_this());
            // if ok
            char buf[1024] = {0};
            snprintf(buf,1024,
                "HTTP/1.1 200 OK\r\n");

            //int fd = _socket.native_handle();

            _socket.async_send(
                buffer(buf, strlen(buf)),
                [self](boost::system::error_code ec, std::size_t ) {
                    //cout << "send http ok to " << fd << endl;
                    if(!ec) {
                        std::cerr << "Accept error: " << ec.message() << std::endl;
                    }
                });
            
            string uri = "." + _env["REQUEST_URI"].substr(0,_env["REQUEST_URI"].find("?"));

            //dup2(fd,STDOUT_FILENO);
            //dup2(fd,STDIN_FILENO);
            
            if(execlp(uri.c_str(),uri.c_str(),(char*)0)==-1) {
                cout << errno << endl;
            }
            //exit(0);
        }
};

class TcpConnection {
    private:
        ip::tcp::acceptor _acceptor;
        ip::tcp::socket _socket;
        boost::asio::signal_set _signal;
    public:
        TcpConnection(short port)
            : _acceptor(ioservice, ip::tcp::endpoint(tcp::v4(), port)),
              _socket(ioservice),
              _signal(ioservice, SIGCHLD) {
            wait_for_signal();
            do_accept();
        }
    private:
    void wait_for_signal() {
        _signal.async_wait(
            [this](boost::system::error_code /*ec*/, int /*signo*/)
            {
            // Only the parent process should check for this signal. We can
            // determine whether we are in the parent by checking if the acceptor
            // is still open.
            if (_acceptor.is_open()) {
                // Reap completed child processes so that we don't end up with
                // zombies.
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
                        // Inform the io_context that the fork is finished and that this
                        // is the child process. The io_context uses this opportunity to
                        // create any internal file descriptors that must be private to
                        // the new process.
                        ioservice.notify_fork(boost::asio::io_service::fork_child);
                        
                        // The child won't be accepting new connections, so we can close
                        // the acceptor. It remains open in the parent.
                        _acceptor.close();
                        
                        // The child process is not interested in processing the SIGCHLD
                        // signal.
                        _signal.cancel();

                        // redirect standard output to socket
                        int fd = _socket.native_handle();
                        dup2(fd,STDOUT_FILENO);
                        dup2(fd,STDIN_FILENO);

                        // run http server to deal with http request
                        make_shared<HttpServer>(move(_socket))->start();
                    } else {
                        // Inform the io_context that the fork is finished (or failed)
                        // and that this is the parent process. The io_context uses this
                        // opportunity to recreate any internal resources that were
                        // cleaned up during preparation for the fork.
                        ioservice.notify_fork(boost::asio::io_context::fork_parent);
                        
                        // The parent process can now close the newly accepted socket. It
                        // remains open in the child.
                        _socket.close();

                        //cout << "accept tcp connection!" << endl; 
                        do_accept();
                    }
                } else {
                    std::cerr << "Accept error: " << ec.message() << std::endl;
                    do_accept();
                }
                
            });
        }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage:" << argv[0] << " [port] " << endl;
        return 1;
    }

    try {
        port = atoi(argv[1]);
        TcpConnection server(port);
        ioservice.run();
    } catch (exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}