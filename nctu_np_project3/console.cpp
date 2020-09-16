#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <map>
#include <unistd.h> // fork

#include <boost/asio.hpp> 
#include <boost/array.hpp> 
#include <boost/asio/ip/tcp.hpp>

#include <memory>
#include <utility>
using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

boost::asio::io_service ioservice; 

typedef struct {
    string _id;
    string _host;
    string _port;
    string _file;
} host_info;

class NpShellRun : public enable_shared_from_this<NpShellRun> {
    private:
        boost::asio::ip::tcp::socket _sock;
        boost::array<char, 4096> buffer;
        ifstream _fin;
        vector<string> command;
        host_info _host_info;
    public:
        NpShellRun(ip::tcp::socket socket, host_info info)  : 
            _sock(move(socket)),
            _host_info(info) {
            //start();
        }
        void start() {
            _fin.open("test_case/" + _host_info._file);
            string line;
            while(getline(_fin, line)) {
                command.push_back(line);
            }
            _fin.close();
            do_read();
        }
    private:
        void do_read() {
            auto self(shared_from_this());
            _sock.async_read_some(boost::asio::buffer(buffer), 
                [this, self](const boost::system::error_code &ec, std::size_t bytes_transferred) {
                    if (!ec) { 
                        //std::cout << std::string(buffer.data(), bytes_transferred) << std::endl; 
                        string response = std::string(buffer.data(), bytes_transferred);
                        encode(response);
                        write_to_console(response);
                        if(command.size()>0) {
                            if((response.find("%")!=string::npos)) {
                                string line = command.at(0);
                                string console_line = line;
                                encode(console_line);
                                write_to_console(console_line);
                                write_to_console(" &NewLine;");
                                line += "\n";
                                boost::asio::write(_sock, boost::asio::buffer(line));
                                command.erase(command.begin());
                            } 
                        }
                        do_read();
                    } 
                });
        }
        void write_to_console(string response) {
            //auto self(shared_from_this());
            // put npshell response to web console
            int pos=0;
            while((pos=response.find("\n",0))!=string::npos) {
            response = response.replace(pos,1,"&NewLine;");
            }
            while((pos=response.find("\r",0))!=string::npos) {
                response = response.replace(pos,1,"");
                }
            cout << "<script>document.getElementById('"<< _host_info._id <<"').innerHTML += \""<< response << "\";</script>" << endl;
        }
        void encode(std::string& data) {
            std::string buffer;
            buffer.reserve(data.size());
            for(size_t pos = 0; pos != data.size(); ++pos) {
                switch(data[pos]) {
                    case '&':  buffer.append("&amp;");       break;
                    case '"': buffer.append("&quot;");      break;
                    case '\'': buffer.append("&apos;");      break;
                    case '<':  buffer.append("&lt;");        break;
                    case '>':  buffer.append("&gt;");        break;
                    default:   buffer.append(&data[pos], 1); break;
                }
            }
            data.swap(buffer);
        }
};

class NpShellConnect : public enable_shared_from_this<NpShellConnect>{
    private:
        host_info _host_info;
        boost::asio::ip::tcp::resolver::query _query;
        boost::asio::ip::tcp::resolver _resolver;
        boost::asio::ip::tcp::socket _sock;
    public:
        NpShellConnect(host_info info) : 
            _host_info(info),
            _query(info._host,info._port),
            _resolver(ioservice),
            _sock(ioservice) {
            //do_connect();
        }
        void start() { do_connect(); }
    private:
        void do_connect() {
            auto self(shared_from_this());
            _resolver.async_resolve(_query, [this,self](const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator it) {
                if (!ec) { 
                    _sock.async_connect(*it, [this,self](const boost::system::error_code &ec) {
                        if (!ec) {
                            //cout << _sock.native_handle() << endl;
                            //make_shared<NpShellRun>(move(_sock), move(_host_info))->start();
                            make_shared<NpShellRun>(move(_sock), move(_host_info))->start();
                        } else {
                            cout << "ERROR: " << ec.message() << endl;
                        }
                    }); 
                } else {
                    cout << "ERROR: " << ec.message() << endl;
                }
            });  
        };
};

void output_shell(string session,string content) {
    cout << "<script>document.getElementById('" << session << "').innerHTML += '"<< content << "';</script>" ;
}

void output_command(string session,string content) {
    cout << "<script>document.getElementById('" << session << "').innerHTML += '<b>"<< content << "&NewLine;</b>';</script>";
}

int main() {
    /* [Required] HTTP Header */
    //cout << "Content-type: text/html "<< endl;
   
    /* [Required] HTTP Header */

    map<string, string> query;
    map<string, int> host_status[5];

    enum { exist = 1, not_exist = -1 };

    string queryString=getenv("QUERY_STRING");
    regex pattern("[fhp][0-9]{1}=[0-9.a-z]*");
    smatch queryToken;

    while (regex_search(queryString, queryToken, pattern)) {
        for (string x:queryToken) {
            query[x.substr(0,2)] = x.substr(3);
            //cout << x.substr(0,2) << " ";
            //cout << query[x.substr(0,2)] << endl;
        } 
        queryString = queryToken.suffix().str();
    }
    cout << "Content-type: text/html" << endl << endl;
    cout << "<!DOCTYPE html>" << endl;
    cout << "<html lang=\"en\"> " << endl;
    cout << "<head> " << endl;
    cout << "<meta charset=\"UTF-8\" /> " << endl;
    cout << "<title>NP Project 3 Console</title> " << endl;
    cout << "<link " << endl;
    cout << "rel=\"stylesheet\" " << endl;
    cout << "href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\" " << endl;
    cout << "integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\" " << endl;
    cout << "crossorigin=\"anonymous\" " << endl;
    cout << "/> " << endl;
    cout << "<link " << endl;
    cout << "href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\" " << endl;
    cout << "rel=\"stylesheet\" " << endl;
    cout << "/> " << endl; 
    cout << "<link " << endl;
    cout << "rel=\"icon\" " << endl;
    cout << "type=\"image/png\" " << endl;
    cout << "href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\" " << endl;
    cout << "/> " << endl; 
    cout << "<style> " << endl;
    cout << "  * { " << endl;
    cout << "    font-family: 'Source Code Pro', monospace; " << endl;
    cout << "    font-size: 1rem !important; " << endl;
    cout << "  } " << endl;
    cout << "  body { " << endl;
    cout << "    background-color: #212529; " << endl;
    cout << "  } " << endl;
    cout << "  pre { " << endl;
    cout << "    color: #cccccc; " << endl;
    cout << "  } " << endl;
    cout << "  b { "  << endl;
    cout << "    color: #ffffff; " << endl;
    cout << "  } " << endl;
    cout << "</style> " << endl;
    cout << "</head> " << endl;
    

    // show the terminal
    cout << "<body>"
        "<table class=\"table table-dark table-bordered\">"
          "<thead>"
            "<tr>";

    for(int i=0; i<5; i++) {
      string h = "h" + to_string(i);
      string p = "p" + to_string(i);
      string f = "f" + to_string(i);
      if(query[h]!="") {
        if(query[p]!="" && query[f]!="")
          cout << "<th scope=\"col\">" << query[h] <<  ":" << query[p] << "</th>" << endl;
        else
          cout << "<th scope=\"col\">" << "LOSS [PORT] or [FILE]" << "</th>" << endl;
      }
    }

    cout << "</tr>"
      "</thead>"
      "<tbody>"
        "<tr>";
    
    for(int i=0; i<5; i++) {
      string h = "h" + to_string(i);
      string p = "p" + to_string(i);
      string f = "f" + to_string(i);
      if(query[h]!="") {
        cout << "<td><pre id=" << h << " class=\"mb-0\"></pre></td>" << endl;
      }
    }

    cout <<  "</tr>"
      "</tbody>"
      "</table>"
      "</body>"
      "</html>";

    for(int i=0; i<5; i++) {
      string h = "h" + to_string(i);
      string p = "p" + to_string(i);
      string f = "f" + to_string(i);
      if(query[h]!="" && query[p]!="" && query[f]!="") {
           try {
                host_info info;
                info._id = h;
                info._host = query[h];
                info._port = query[p];
                info._file = query[f];
                make_shared<NpShellConnect>(move(info))->start();
            } catch (exception& e) {
                cerr << "Exception: " << e.what() << "\n";
            }
        }
    }
    ioservice.run();
    //NpShellConnect.use_count();

    return 0;
}

