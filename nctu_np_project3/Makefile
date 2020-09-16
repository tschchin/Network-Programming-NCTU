all:
	clang++ -std=c++11 http_server.cpp -o http_server -I /usr/local/include -L /usr/local/lib -Wall -pedantic -pthread -lboost_system
	clang++ -std=c++11 -Wall console.cpp -o console.cgi -I /usr/local/include -L /usr/local/lib -Wall -pedantic -pthread -lboost_system
