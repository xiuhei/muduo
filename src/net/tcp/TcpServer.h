#pragma once

#include "base/NonCopyable.h"
#include "net/tcp/Callbacks.h"
#include "net/tcp/Acceptor.h"
#include "net/reactor/EventLoopThreadPool.h"
#include "net/timer/Timer.h"
#include "net/timer/TimerId.h"

#include<map>
#include<memory>
#include<string>
#include<functional>
#include<atomic>
#include<chrono>
namespace muduo
{

class EventLoop;
class InetAddress;
class EventLoopThreadPool;


// TCP 服务入口：base Reactor 负责监听和连接表，IO Reactor 负责连接读写。
class TcpServer : public NonCopyable {
public:
    using TimerCallback = std::function<void()>;

    TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& name);
    ~TcpServer();
    void start();
    // 停止接收新连接，等待现有连接关闭；超过 gracePeriod 后强制关闭。
    // completed 始终在 base EventLoop 线程执行。
    void stop(Duration gracePeriod = std::chrono::seconds(10),
              std::function<void()> completed = {});

    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback& cb){connectionCallback_ =cb;}

    void setThreadNum(int numThreads);

    // 设置连接空闲超时（秒），0 表示不启用
    void setConnectionIdleTimeout(std::chrono::seconds timeout) { connectionIdleTimeout_ = timeout; }
    // 设置连接写入超时，0 表示不启用
    void setConnectionWriteTimeout(Duration timeout) { connectionWriteTimeout_ = timeout; }

    // 定时器接口，委托给 base EventLoop。线程安全（可在其他线程调用）
    TimerId runAt(TimePoint time, TimerCallback cb);
    TimerId runAfter(Duration delay, TimerCallback cb);
    TimerId runEvery(Duration interval, TimerCallback cb);
    void cancelTimer(TimerId timerId);

private:
    // new/remove 在 base loop 修改连接表，连接事件在选定的 IO loop 执行。
    void newConnection(int sockfd, const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    void stopInLoop(Duration gracePeriod, std::function<void()> completed);
    void finishStop();

    EventLoop* loop_;                                      // base Reactor，不拥有。
    std::string name_;
    std::unique_ptr<Acceptor> acceptor_;                   // 监听新连接。
    std::unique_ptr<EventLoopThreadPool> threadPool_;
    std::atomic<bool> started_{false};                      //是否已启动
    std::atomic<bool> stopping_{false};
    int nextConnId_{1};                                     //连接 ID 生成器
    std::map<std::string,TcpConnectionPtr> connections_;
    
    MessageCallback messageCallback_;
    ConnectionCallback connectionCallback_;

    std::chrono::seconds connectionIdleTimeout_{0};
    TimerId idleCheckTimerId_;
    Duration connectionWriteTimeout_{0};
    TimerId forceCloseTimerId_;
    std::function<void()> stopCompleted_;

};

}// namespace muduo
