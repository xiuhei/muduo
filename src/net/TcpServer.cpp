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
        loop_->runInLoop([this] {
            acceptor_->listen();
            // 如果配置了空闲超时或写入超时，启动周期性扫描定时器
            if (connectionIdleTimeout_.count() > 0 || connectionWriteTimeout_.count() > 0) {
                idleCheckTimerId_ = loop_->runEvery(std::chrono::seconds(3), [this] {
                    // 每 3 秒扫描所有连接，关闭空闲/写入超时的连接
                    TimePoint now = std::chrono::steady_clock::now();
                    for (auto it = connections_.begin(); it != connections_.end(); ) {
                        const auto& conn = it->second;
                        if (conn->isIdleTimeout(now) || conn->isWriteTimeout(now)) {
                            conn->shutdown();
                        }
                        ++it;
                    }
                });
            }
        });
    }
}
TcpServer::~TcpServer() {
    // 取消周期性空闲扫描定时器，防止 TcpServer 析构后定时器回调访问野指针
    loop_->cancel(idleCheckTimerId_);
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
    if (connectionIdleTimeout_.count() > 0) {
        conn->setIdleTimeout(connectionIdleTimeout_);
    }
    if (connectionWriteTimeout_.count() > 0) {
        conn->setWriteTimeout(connectionWriteTimeout_);
    }
    std::cout << "new connection " << connName << " from " << peerAddr.toIpPort() << '\n';
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
    loop_->runInLoop([this, conn] {
        connections_.erase(conn->name());
        EventLoop* ioLoop = conn->getLoop();
        ioLoop->queueInLoop([conn] { conn->connectDestroyed(); });
    });
}

// 定时器接口，直接委托给 base EventLoop
TimerId TcpServer::runAt(TimePoint time, TimerCallback cb)
{
    return loop_->runAt(time, std::move(cb));
}

TimerId TcpServer::runAfter(Duration delay, TimerCallback cb)
{
    return loop_->runAfter(delay, std::move(cb));
}

TimerId TcpServer::runEvery(Duration interval, TimerCallback cb)
{
    return loop_->runEvery(interval, std::move(cb));
}

void TcpServer::cancelTimer(TimerId timerId)
{
    loop_->cancel(timerId);
}

}//namespace muduo
