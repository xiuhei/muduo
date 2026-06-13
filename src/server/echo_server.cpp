#include"net/EventLoop.h"
#include"net/TcpServer.h"
#include"net/InetAddress.h"
#include"net/TcpConnection.h"

#include<iostream>

int main(int argc, char* argv[]){
    muduo::EventLoop mainLoop;
    muduo::TcpServer server(&mainLoop, muduo::InetAddress(8888),std::string("EchoServer"));
    
    int threadNum=4;
    if(argc>1){threadNum=std::stoi(argv[1]);}
    server.setThreadNum(threadNum);
    server.setConnectionCallback([](const muduo::TcpConnectionPtr& conn) {
        std::cout << (conn->connected() ? "connected " : "disconnected ")
                  << conn->name() << " peer=" << conn->peerAddress().toIpPort() << '\n';
    });
    server.setMessageCallback([](const muduo::TcpConnectionPtr& conn, muduo::Buffer* buffer) {
        std::string message = buffer->retrieveAllAsString();
        conn->send(message);
    });

    server.start();
    std::cout << "echo_server listening on 0.0.0.0:8888 with "<< threadNum<< " IO threads\n";
    mainLoop.loop();
    

}