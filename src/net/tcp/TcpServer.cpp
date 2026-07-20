#include "net/tcp/TcpServer.h"
#include "net/tcp/TcpConnection.h"
#include "net/reactor/EventLoopThreadPool.h"
#include "net/tcp/Acceptor.h"
#include "net/reactor/EventLoop.h"
#include "base/Logger.h"

#include<memory>
#include <cerrno>
#include <cstring>
#include <unistd.h>


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
        LOG_INFO("starting TCP server {}", name_);
        // listen fd 属于 base loop，监听动作也投递回 base loop 执行。
        loop_->runInLoop([this] {
            acceptor_->listen();
            // 只有监听 socket 成功后才启动 IO 线程；bind/listen 失败会抛出并终止启动。
            threadPool_->start();
            // 如果配置了空闲超时或写入超时，启动周期性扫描定时器
            if (connectionIdleTimeout_.count() > 0 || connectionWriteTimeout_.count() > 0) {
                idleCheckTimerId_ = loop_->runEvery(std::chrono::seconds(3), [this] {
                    // 每 3 秒扫描所有连接，关闭空闲/写入超时的连接
                    TimePoint now = std::chrono::steady_clock::now();
                    for (auto it = connections_.begin(); it != connections_.end(); ) {
                        const auto& conn = it->second;
                        if (conn->isIdleTimeout(now) || conn->isWriteTimeout(now)) {
                            LOG_WARN("connection {} exceeded configured timeout; shutting down",
                                     conn->name());
                            conn->shutdown();
                        }
                        ++it;
                    }
                });
            }
            LOG_INFO("TCP server {} started", name_);
        });
    }
}
TcpServer::~TcpServer() {
    loop_->assertInLoopThread();
    LOG_INFO("stopping TCP server {} with {} active connections", name_, connections_.size());
    acceptor_->stop();
    // 取消周期性空闲扫描定时器，防止 TcpServer 析构后定时器回调访问野指针
    loop_->cancel(idleCheckTimerId_);
    loop_->cancel(forceCloseTimerId_);
    for (auto& [name, conn] : connections_) {
        conn->setCloseCallback({});
        EventLoop* ioLoop = conn->getLoop();
        ioLoop->runInLoop([conn] { conn->connectDestroyed(); });
    }
}

void TcpServer::stop(Duration gracePeriod, std::function<void()> completed) {
    loop_->runInLoop([this, gracePeriod, completed = std::move(completed)]() mutable {
        stopInLoop(gracePeriod, std::move(completed));
    });
}

void TcpServer::stopInLoop(Duration gracePeriod, std::function<void()> completed) {
    loop_->assertInLoopThread();
    if (stopping_.exchange(true)) {
        return;
    }

    stopCompleted_ = std::move(completed);
    acceptor_->stop();
    loop_->cancel(idleCheckTimerId_);
    LOG_INFO("draining TCP server {} with {} active connections", name_, connections_.size());

    if (connections_.empty()) {
        finishStop();
        return;
    }

    // shutdown 任务排在此前已经投递给相同 IO loop 的业务任务之后。
    for (const auto& entry : connections_) {
        entry.second->shutdown();
    }

    if (gracePeriod <= Duration::zero()) {
        for (const auto& entry : connections_) entry.second->forceClose();
    } else {
        forceCloseTimerId_ = loop_->runAfter(gracePeriod, [this] {
            LOG_WARN("graceful shutdown timeout for {}; force closing {} connections",
                     name_, connections_.size());
            for (const auto& entry : connections_) entry.second->forceClose();
        });
    }
}

void TcpServer::finishStop() {
    loop_->assertInLoopThread();
    loop_->cancel(forceCloseTimerId_);
    LOG_INFO("TCP server {} drained all connections", name_);
    auto completed = std::move(stopCompleted_);
    if (completed) completed();
}

void  TcpServer::setThreadNum(int numThreads) {
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) {
    loop_->assertInLoopThread();
    if (stopping_) {
        LOG_INFO("rejecting connection from {} while server is stopping", peerAddr.toIpPort());
        ::close(sockfd);
        return;
    }
    EventLoop *ioLoop=threadPool_->getNextLoop();
    std::string connName = name_ + "#" + std::to_string(nextConnId_++);

    sockaddr_in local{};
    socklen_t addrlen = sizeof(local);
    if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&local), &addrlen) < 0) {
        LOG_ERROR("getsockname failed for fd={}: {}", sockfd, std::strerror(errno));
    }
    InetAddress localAddr(local);

    auto conn=std::make_shared<TcpConnection>(ioLoop, connName, sockfd, localAddr, peerAddr);
    // 连接表归 base loop 管理；连接的 Channel 与缓冲区归选定的 IO loop 管理。
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
    LOG_INFO("new connection {} from {}", connName, peerAddr.toIpPort());
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
    // close 回调来自 IO loop，先切回 base loop 修改连接表，再回 IO loop 注销 Channel。
    loop_->runInLoop([this, conn] {
        LOG_DEBUG("removing connection {}", conn->name());
        connections_.erase(conn->name());
        EventLoop* ioLoop = conn->getLoop();
        ioLoop->queueInLoop([conn] { conn->connectDestroyed(); });
        if (stopping_ && connections_.empty()) {
            finishStop();
        }
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
