#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>
#include <string>
#include <ctime> 
#include <boost/asio/signal_set.hpp>
#include <sys/types.h>
#include <iostream>
#include <memory>
#include <utility>
#include <array>
#include <regex>
#include <map>
#include <fstream>
#include <boost/array.hpp> 

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

io_service ioservice;

typedef struct {
	string _id;
	string _host;
	string _port;
	string _file;
} host_info;

class NpShellRun : public enable_shared_from_this<NpShellRun> {
private:
	boost::asio::ip::tcp::socket _sock;
	shared_ptr<ip::tcp::socket> _console_sock;
	boost::array<char, 4096> buffer;
	vector<string> command;
	string _id;
	string _host;
	string _port;
	string _file;
public:
	NpShellRun(const shared_ptr<ip::tcp::socket> &console_sock, ip::tcp::socket socket, string id, string host, string port, string file) :
		_id(id),
		_host(move(host)),
		_port(move(port)),
		_file(move(file)),
		_console_sock(console_sock),
		_sock(move(socket))
	{
		//start();
	}
	void start() {
		cout << "!!!!" << endl;
		ifstream fin;
		string line;
		cout << _file << endl;
		fin.open("./test_case/" + _file);
		if (!fin) {
			cout << "File Open Error" << endl;
		}
		while (getline(fin, line)) {
			command.push_back(line);
			cout << line << endl;
		}
		fin.close();
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
				string line;

				if (command.size() != 0) {
					if ((response.find("%") != string::npos)) {
						string line = command.at(0);
						string line_console = line;
						encode(line_console);
						write_to_console(line_console);
						write_to_console(" &NewLine;");
                        line += "\n";
						//cout << "do read : write" << _sock.native_handle() << endl;
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
		int pos = 0;
		while ((pos = response.find("\n", 0)) != string::npos) {
			response = response.replace(pos, 1, "&NewLine;");
		}
		while ((pos = response.find("\r", 0)) != string::npos) {
			response = response.replace(pos, 1, "");
		}
		string insert = "<script>document.getElementById('" + _id + "').innerHTML += \"" + response + "\";</script>";
		//try {
		boost::asio::write(*_console_sock, boost::asio::buffer(insert));
		//}
		//catch (exception& e) {
		//	std::cerr << "Exception: " << e.what() << "\n";
		//}
	}
	void encode(std::string& data) {
		std::string buffer;
		buffer.reserve(data.size());
		for (size_t pos = 0; pos != data.size(); ++pos) {
			switch (data[pos]) {
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

class NpShellConnect : public enable_shared_from_this<NpShellConnect> {
private:
	string _id;
	string _host;
	string _port;
	string _file;
	boost::asio::ip::tcp::resolver::query _query;
	boost::asio::ip::tcp::resolver _resolver;
	boost::asio::ip::tcp::socket _sock;

	shared_ptr<ip::tcp::socket> _console_sock;
public:
	NpShellConnect(const shared_ptr<ip::tcp::socket> &console_sock, string id, string host, string port, string file) :
		_id(id),
		_host(move(host)),
		_port(move(port)),
		_file(move(file)),
		_query(_host, _port),
		_resolver(ioservice),
		_sock(ioservice),
		_console_sock(console_sock) {
		//do_connect();
		//cout << info._host << " " << info._port << endl;
	}
	void start() { do_connect(); }
private:
	void do_connect() {
		auto self(shared_from_this());
		_resolver.async_resolve(_query, [this, self](const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator it) {
			if (!ec) {
				_sock.async_connect(*it, [this, self](const boost::system::error_code &ec) {
					if (!ec) {
						//cout << _sock.native_handle() << endl;
						//make_shared<NpShellRun>(move(_sock), move(_host_info))->start();

						make_shared<NpShellRun>(_console_sock, move(_sock), move(_id), move(_host), move(_port), move(_file))->start();
						//boost::asio::write(_console_sock, boost::asio::buffer("hello"));
					}
					else {
						cout << "ERROR: " << ec.message() << endl;
					}
				});
			}
			else {
				cout << "ERROR: " << ec.message() << endl;
			}
		});
	};
};


class HttpServer : public enable_shared_from_this<HttpServer> {
private:
	enum { max_length = 1024 };
	ip::tcp::socket _socket;
	array<char, max_length> _data;
	map<string, string> _env;
	host_info info;
public:
	HttpServer(ip::tcp::socket socket) : _socket(move(socket)) {
		_env["REMOTE_ADDR"] = _socket.remote_endpoint().address().to_string();
		_env["REMOTE_PORT"] = to_string(_socket.remote_endpoint().port());
		_env["SERVER_ADDR"] = _socket.local_endpoint().address().to_string();;
		_env["SERVER_PORT"] = to_string(_socket.local_endpoint().port());;
	}
	void start() { do_read(); }
private:
	void do_read() {
		auto self(shared_from_this());
		_socket.async_read_some(
			buffer(_data, max_length),
			[this, self](boost::system::error_code ec, std::size_t length) {
			if (!ec) {
				string req("");
				for (auto x : _data)
					req += x;
				parseHttpRequest(req);
				//setEnv();
				connect_to_url();
				//exit(0);
			}
		});
	}

	void parseHttpRequest(string req) {
		// get http request header
		string line_delimiter = "\n";
		string req_header = req.substr(0, req.find(line_delimiter));

		// get http host
		req.erase(0, req.find(line_delimiter) + line_delimiter.length());
		string host_line = req.substr(0, req.find(line_delimiter));
		_env["HTTP_HOST"] = host_line.substr(req.find(" ") + 1, host_line.length());
		//cout << _env["HTTP_HOST"] << endl;

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
		_env["REQUEST_URI"] = uri_and_query;//.substr(0, uri_and_query.find("?"));
		if (uri_and_query.find("?") != string::npos)
			_env["QUERY_STRING"] = uri_and_query.substr(uri_and_query.find("?") + 1, uri_and_query.length());
		else
			_env["QUERY_STRING"] = "";
		req_header.erase(0, pos + header_delimiter.length());
		//cout << _env["REQUEST_URI"] << endl;
		//cout << _env["QUERY_STRING"] << endl;

		// Server Protocol
		_env["SERVER_PROTOCOL"] = req_header;
		//cout << _env["SERVER_PROTOCOL"] << endl;
	}

	void setEnv() {
		/*setenv("REQUEST_METHOD", _env["REQUEST_METHOD"].c_str(), 1);
		setenv("REQUEST_URI", _env["REQUEST_URI"].c_str(), 1);
		setenv("QUERY_STRING", _env["QUERY_STRING"].c_str(), 1);
		setenv("SERVER_PROTOCOL", _env["SERVER_PROTOCOL"].c_str(), 1);
		setenv("HTTP_HOST", _env["HTTP_HOST"].c_str(), 1);
		setenv("SERVER_ADDR", _env["SERVER_ADDR"].c_str(), 1);
		setenv("SERVER_PORT", _env["SERVER_PORT"].c_str(), 1);
		setenv("REMOTE_ADDR", _env["REMOTE_ADDR"].c_str(), 1);
		setenv("REMOTE_PORT", _env["REMOTE_PORT"].c_str(), 1);*/
	}

	void connect_to_url() {
		auto self(shared_from_this());
		// if ok
		char buf[1024] = { 0 };
		snprintf(buf, 1024,
			"HTTP/1.1 200 OK\r\n"
			"Content-type: text/html\r\n\r\n\n");
		_socket.async_send(
			buffer(buf, strlen(buf)),
			[self](boost::system::error_code ec, std::size_t) {
			if (!ec) {
				//std::cerr << "Accept error: " << ec.message() << std::endl;
			}
		});

        string uri =  _env["REQUEST_URI"].substr(0,_env["REQUEST_URI"].find("?"));
		if (uri == "/panel.cgi") {
			run_panel_cgi();
		}
		else if (uri == "/console.cgi") {
			run_console_cgi();
		}
		else {
			boost::asio::write(_socket, boost::asio::buffer("NOT FOUND"));
		}
	}

	void run_panel_cgi() {
		string header = "<!DOCTYPE html>"
			"<html lang = \"en\">"
			"<head>"
			"<title>NP Project 3 Panel</title>"
			"<link "
			"rel = \"stylesheet\""
			"href =\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\""
			"integrity = \"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\""
			"crossorigin = \"anonymous\""
			"/>"
			"<link "
			"href = \"https://fonts.googleapis.com/css?family=Source+Code+Pro\""
			"rel = \"stylesheet\""
			"/> "
			"<link "
			"rel = \"icon\""
			"type = \"image/png\""
			"href = \"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\""
			"/> "
			"<style> "
			"* {"
			"font - family: 'Source Code Pro', monospace;"
			"}"
			"</style> "
			"</head> ";
		string body = "<body class=\"bg - secondary pt - 5\"> "
			"<form action = \"console.cgi\" method = \"GET\">"
			"<table class = \"table mx-auto bg-light\" style = \"width: inherit\">"
			"<thead class = \"thead-dark\">"
			"<tr>"
			"<th scope = \"col\">#</th> "
			"<th scope = \"col\">Host</th> "
			"<th scope = \"col\">Port</th> "
			"<th scope = \"col\">Input File</th> "
			"</tr>"
			"</thead>"
			"<tbody> ";
		string panel = "";
		for (int i = 0; i < 5; i++) {
			panel += "<tr> "
				"<th scope = \"row\" class = \"align - middle\">Session " + to_string(i + 1) + " </th> "
				"<td> "
				"<div class = \"input-group\"> "
				"<select name = \"h" + to_string(i) + "\" class = \"custom-select\"> "
				"<option></option><option value = \"nplinux1.cs.nctu.edu.tw\">nplinux1</option><option value = \"nplinux2.cs.nctu.edu.tw\">nplinux2</option><option value = \"nplinux3.cs.nctu.edu.tw\">nplinux3</option><option value = \"nplinux4.cs.nctu.edu.tw\">nplinux4</option><option value = \"nplinux5.cs.nctu.edu.tw\">nplinux5</option><option value = \"npbsd1.cs.nctu.edu.tw\">npbsd1</option><option value = \"npbsd2.cs.nctu.edu.tw\">npbsd2</option><option value = \"npbsd3.cs.nctu.edu.tw\">npbsd3</option><option value = \"npbsd4.cs.nctu.edu.tw\">npbsd4</option><option value = \"npbsd5.cs.nctu.edu.tw\">npbsd5</option> "
				"</select>"
				"<div class = \"input-group-append\">"
				"<span class = \"input-group-text\">.cs.nctu.edu.tw</span> "
				"</ div>"
				"</div> "
				"</td> "
				"<td> "
				"<input name = \"p" + to_string(i) + "\" type = \"text\" class = \"form-control\" size = \"5\" /> "
				"</td> "
				"<td> "
				"<select name = \"f" + to_string(i) + "\" class = \"custom-select\"> "
				"<option></option>"
				"<option value = \"t1.txt\">t1.txt</option><option value = \"t2.txt\">t2.txt</option><option value = \"t3.txt\">t3.txt</option><option value = \"t4.txt\">t4.txt</option><option value = \"t5.txt\">t5.txt</option><option value = \"t6.txt\">t6.txt</option><option value = \"t7.txt\">t7.txt</option><option value = \"t8.txt\">t8.txt</option><option value = \"t9.txt\">t9.txt</option><option value = \"t10.txt\">t10.txt</option> "
				"</select> "
				"</td>"
				"</tr>"
				"<tr>";
		}
		string tail = "<tr> "
			"<td colspan = \"3\"></td>"
			"<td>"
			"<button type = \"submit\" class = \"btn btn - info btn - block\">Run</button> "
			"</td> "
			"</tr> "
			"</tbody> "
			"</table> "
			"</form> "
			"</body> "
			"</html> ";

		boost::asio::write(_socket, boost::asio::buffer(header));
		boost::asio::write(_socket, boost::asio::buffer(body));
		boost::asio::write(_socket, boost::asio::buffer(panel));
		boost::asio::write(_socket, boost::asio::buffer(tail));

	}

	void parse_query_string(map<string, string> &query, string queryString) {
		map<string, int> host_status[5];

		enum { exist = 1, not_exist = -1 };

		//string queryString = getenv("QUERY_STRING");
		regex pattern("[fhp][0-9]{1}=[0-9.a-z]*");
		smatch queryToken;

		while (regex_search(queryString, queryToken, pattern)) {
			for (string x : queryToken) {
				query[x.substr(0, 2)] = x.substr(3);
				//cout << x.substr(0,2) << " ";
				//cout << query[x.substr(0,2)] << endl;
			}
			//cout << endl;
			queryString = queryToken.suffix().str();
		}
	}

	void run_console_cgi() {
		string header = "<!DOCTYPE html> "
			"<html lang=\"en\"> "
			"<head> "
			"<meta charset=\"UTF-8\" /> "
			"<title>NP Project 3 Console</title> "
			"<link "
			"rel=\"stylesheet\" "
			"href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\" "
			"integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\" "
			"crossorigin=\"anonymous\" "
			"/> "
			"<link "
			"href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\" "
			"rel=\"stylesheet\""
			"/> "
			"<link "
			"rel=\"icon\" "
			"type=\"image/png\" "
			"href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\" "
			"/> "
			"<style> "
			"  * {"
			"    font-family: 'Source Code Pro', monospace; "
			"    font-size: 1rem !important; "
			"  } "
			"  body { "
			"    background-color: #212529; "
			"  } "
			"  pre { "
			"    color: #cccccc; "
			"  } "
			"  b { "
			"    color: #ffffff; "
			"  } "
			"</style> "
			"</head> ";

		boost::asio::write(_socket, boost::asio::buffer(header));

		// parse query string
		map<string, string> parsed_query_string;
		parse_query_string(parsed_query_string, _env["QUERY_STRING"]);

		// show the terminal
		string body = "<body> "
			"<table class=\"table table-dark table-bordered\"> "
			"<thead> "
			"<tr> ";

		for (int i = 0; i < 5; i++) {
			string h = "h" + to_string(i);
			string p = "p" + to_string(i);
			string f = "f" + to_string(i);
			if (parsed_query_string[h] != "") {
				if (parsed_query_string[p] != "" && parsed_query_string[f] != "")
					//cout << "<th scope=\"col\">" << parsed_query_string[h] << ":" << parsed_query_string[p] << "</th>" << endl;
					body = body + "<th scope=\"col\">" + parsed_query_string[h] + ":" + parsed_query_string[p] + "</th>\n";
				else
					//cout << "<th scope=\"col\">" << "LOSS [PORT] or [FILE]" << "</th>" << endl;
					body = body + "<th scope=\"col\">" + "LOSS [PORT] or [FILE]" + "</th>\n";
			}
		}

		body += "</tr> "
			"</thead> "
			"<tbody> "
			"<tr> ";

		for (int i = 0; i < 5; i++) {
			string h = "h" + to_string(i);
			string p = "p" + to_string(i);
			string f = "f" + to_string(i);
			if (parsed_query_string[h] != "") {
				body = body + "<td><pre id=" + h + " class=\"mb-0\"></pre></td>\n";
			}
		}

		body += "</tr> "
			"</tbody> "
			"</table> "
			"</body> "
			"</html> ";

		boost::asio::write(_socket, boost::asio::buffer(body));

		auto share_ptr_sock = make_shared<ip::tcp::socket>(move(_socket));

		for (int i = 0; i < 5; i++) {
			string h = "h" + to_string(i);
			string p = "p" + to_string(i);
			string f = "f" + to_string(i);
			if (parsed_query_string[h] != "" && parsed_query_string[p] != "" && parsed_query_string[f] != "") {
				try {
					//host_info info;
					string _id = h;
					string _host = parsed_query_string[h];
					string _port = parsed_query_string[p];
					string _file = parsed_query_string[f];

					make_shared<NpShellConnect>(share_ptr_sock, move(_id), move(_host), move(_port), move(_file))->start();
				}
				catch (exception& e) {
					cerr << "Exception: " << e.what() << "\n";
				}
			}
		}
	}
};

class TcpConnection {
private:
	ip::tcp::acceptor _acceptor;
	ip::tcp::socket _socket;
public:
	TcpConnection(short port)
		: _acceptor(ioservice, ip::tcp::endpoint(tcp::v4(), port)),
		_socket(ioservice) {
		do_accept();
	}
private:
	void do_accept() {
		_acceptor.async_accept(_socket, [this](boost::system::error_code ec) {
			if (!ec) {
				// run http server to deal with http request
				make_shared<HttpServer>(move(_socket))->start();
				do_accept();
			}
			else {
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
		int port = atoi(argv[1]);
		TcpConnection server(port);
		ioservice.run();
	}
	catch (exception& e) {
		std::cerr << "Exceptionxxxx: " << e.what() << "\n";
	}
	system("pause");
	return 0;
}