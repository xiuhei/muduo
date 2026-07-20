#include"net/reactor/EventLoop.h"
#include"net/tcp/TcpServer.h"
#include"net/tcp/InetAddress.h"
#include"net/tcp/TcpConnection.h"
#include"base/Logger.h"
#include "net/reactor/SignalWatcher.h"

#include<iostream>
#include <csignal>

int main(int argc, char* argv[]){
    muduo::SignalWatcher::block({SIGINT, SIGTERM});
    muduo::LogGuard logGuard("muduo");
    muduo::EventLoop mainLoop;
    muduo::SignalWatcher signals(&mainLoop, {SIGINT, SIGTERM});
    muduo::TcpServer server(&mainLoop, muduo::InetAddress(8888),std::string("EchoServer"));
    
    int threadNum=4;
    if(argc>1){threadNum=std::stoi(argv[1]);}
    server.setThreadNum(threadNum);
    server.setConnectionCallback([](const muduo::TcpConnectionPtr& conn) {
        LOG_INFO("{} {} peer={}", conn->connected() ? "connected" : "disconnected",
                 conn->name(), conn->peerAddress().toIpPort());
    });
    server.setMessageCallback([](const muduo::TcpConnectionPtr& conn, muduo::Buffer* buffer) {
        std::string message = buffer->retrieveAllAsString();
        conn->send(message);
    });

    server.start();
    signals.setCallback([&](int) {
        server.stop(std::chrono::seconds(10), [&mainLoop] { mainLoop.quit(); });
    });
    LOG_INFO("echo_server listening on 0.0.0.0:8888 with {} IO threads", threadNum);
    mainLoop.loop();
    

}
