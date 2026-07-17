#pragma once

#include "base/NonCopyable.h"
#include "net/Callbacks.h"
#include "net/Acceptor.h"
#include "net/EventLoopThreadPool.h"
#include "net/Timer.h"
#include "net/TimerId.h"

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


/*
    Tcpserver 类，对外的服务入口，管理 Acceptor 和所有 TcpConnection
*/
class TcpServer : public NonCopyable {
public:
    using TimerCallback = std::function<void()>;

    TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& name);
    ~TcpServer();
    void start();

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
    void newConnection(int sockfd, const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);

    EventLoop* loop_;                                       //main Reactor
    std::string name_;                                      //name
    std::unique_ptr<Acceptor> acceptor_;                    //acceptor 负责监听新连接事件
    std::unique_ptr<EventLoopThreadPool> threadPool_;
    std::atomic<bool> started_{false};                      //是否已启动
    int nextConnId_{1};                                     //连接 ID 生成器
    std::map<std::string,TcpConnectionPtr> connections_;
    
    MessageCallback messageCallback_;    
    ConnectionCallback connectionCallback_;             //onMessage 回调函数

    std::chrono::seconds connectionIdleTimeout_{0};
    TimerId idleCheckTimerId_;
    Duration connectionWriteTimeout_{0};

};

}// namespace muduo