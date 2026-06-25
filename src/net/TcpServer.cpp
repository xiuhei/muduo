#include "net/TcpServer.h"
#include "net/TcpConnection.h"
#include "net/EventLoopThreadPool.h"
#include "net/Acceptor.h"
#include "net/EventLoop.h"

#include<memory>
#include<iostream>


namespace muduo
{
TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& name)
    : loop_(loop),
      name_(name),
      acceptor_(std::make_unique<Acceptor>(loop, listenAddr)),
      threadPool_(std::make_unique<EventLoopThreadPool>(loop)) {
        acceptor_->setNewConnectionCallback([this](int sockfd,const InetAddress& peerAddr){
            newConnection(sockfd,peerAddr);
        });
      }


void TcpServer::start(){
    if (!started_.exchange(true)) {
        threadPool_->start();
        // listen fd 属于 base loop，监听动作也投递回 base loop 执行。
        loop_->runInLoop([this] { acceptor_->listen(); });
    }
}
TcpServer::~TcpServer() {
    for (auto& [name, conn] : connections_) {
        EventLoop* ioLoop = conn->getLoop();
        ioLoop->runInLoop([conn] { conn->connectDestroyed(); });
    }
}

void  TcpServer::setThreadNum(int numThreads) {
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) {
    loop_->assertInLoopThread();
    EventLoop *ioLoop=threadPool_->getNextLoop();
    std::string connName = name_ + "#" + std::to_string(nextConnId_++);

    sockaddr_in local{};
    socklen_t addrlen = sizeof(local);
    ::getsockname(sockfd, reinterpret_cast<sockaddr*>(&local), &addrlen);
    InetAddress localAddr(local);

    auto conn=std::make_shared<TcpConnection>(ioLoop, connName, sockfd, localAddr, peerAddr);
    connections_[connName]=conn;
    conn->setMessageCallback(messageCallback_);
    conn->setConnectionCallback(connectionCallback_);
    conn->setCloseCallback([this](const TcpConnectionPtr& c){removeConnection(c);});

    ioLoop->runInLoop([conn]{conn->connectEstablished();});
    std::cout << "new connection " << connName << " from " << peerAddr.toIpPort() << '\n';
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
    loop_->runInLoop([this, conn] {
        connections_.erase(conn->name());
        EventLoop* ioLoop = conn->getLoop();
        ioLoop->queueInLoop([conn] { conn->connectDestroyed(); });
    });
}
// TcpConnection.cpp 里加
void TcpConnection::shutdown() {
    if (state_ == State::Connected) {
        setState(State::Disconnecting);
        loop_->runInLoop([this] { shutdownInLoop(); });
    }
}

}//namespace muduo
