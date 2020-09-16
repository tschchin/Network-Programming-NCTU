# 2018 Fall NCTU Network Programming Course Homework

## Project 1 : NPshell
Design a shell with special piping mechanisms.

## Project 2 : Remote Working Ground (rwg) Server
The server of the chat-like systems, called remote working systems (rws). In this system, users can meet with/work with/talk to/make friends with other users. Basically, this system supports all functions as you did in project 1. The first project is the simple connection 
1. Design a **concurrent connection-oriented server**. This server allows one client connect to it, and supports all performance in project 1.
2. RWG server using **single-process concurrent paradigm** 
3. RWG server using **concurrent connection-oriented paradigm with shared memory** 

## Project 3 : Remote Batch System
A remote batch system that runs over HTTP. All the programs in this project are implemented using Boost.Asio. We have two versions: (1) Run on Linux OS and (2) Run on Windows10 OS.
- Design a CGI program connected to project2 server.
- Design a HTTP server handling the connection.  
  
Query for access the server (http://[host]:[port]) : http://nplinux1.cs.nctu.edu.tw:8888/test.cgi?a=b&c=d

## Project 4 : SOCKS4
Implement the SOCKS4 firewall protocol in the application layer of the OSI model.
1. SOCKS4 Server with **Connect Mode**
2. SOCKS4 Server with **Bind Mode** (for **FTP**)
3. CGI Proxy
4. Firewall for IP blocking  
e.g. permit c 140.114.*.*  permit NTHU IP (connect mode)

