#pragma once

#include "base/NonCopyable.h"
#include "net/Socket.h"
#include "net/Channel.h"
#include "net/InetAddress.h"
#include "net/Buffer.h"
#include "net/Callbacks.h"
#include "net/Timer.h"
#include "net/TimerId.h"

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <any>

namespace muduo
{

class EventLoop;    


/*
    一条已建立的 TCP 连接的完整生命周期管理，读写缓冲、状态机、回调
*/
class TcpConnection : public NonCopyable ,public std::enable_shared_from_this<TcpConnection>{
public:
    enum class State { Connecting, Connected, Disconnecting, Disconnected };
    TcpConnection(EventLoop* loop,std::string name, int sockfd, const InetAddress& localAddr, const InetAddress& peerAddr);
    ~TcpConnection();


    void setContext_(const std::any& ctx){context_=ctx;}
    const std::any& getContext() const { return context_; }
    std::any* getMutableContext() { return &context_; }

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }
    bool connected() const { return state_ == State::Connected; }
    
    void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }


    // 空闲超时：连接在 idleTimeout_ 秒内无读写活动则被关闭，0 表示不启用
    void setIdleTimeout(std::chrono::seconds timeout) { idleTimeout_ = timeout; }
    void updateLastActiveTime() { lastActiveTime_ = std::chrono::steady_clock::now(); }
    bool isIdleTimeout(std::chrono::steady_clock::time_point now) const;

    // 写入超时：数据在 outputBuffer_ 中积压超过 writeTimeout_ 则关闭连接，0 表示不启用
    void setWriteTimeout(Duration timeout) { writeTimeout_ = timeout; }
    bool isWriteTimeout(TimePoint now) const;

    //调用 shutdown 后对端超过 shutdownTimeout_ 未发 FIN 则强制关闭
    void setShutdownTimeout(Duration timeout) { shutdownTimeout_ = timeout; }

    void connectEstablished();
    void connectDestroyed();
    void send(std::string message);
    void shutdown();
    // 强制关闭连接，跳过优雅关闭流程（用于超时强制断开）
    void forceClose();
private:
    void setState(State state) { state_ = state; }
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();
    void shutdownInLoop();
    void forceCloseInLoop();
    void sendInLoop(std::string message);

    EventLoop* loop_;                    // 连接所属的 IO Reactor，不拥有该对象。
    const std::string name_;             // 连接名，由 TcpServer 生成，用于日志和连接表索引。
    State state_{State::Connecting};     // 连接生命周期状态，只在所属 loop 线程内流转。
    Socket socket_;                      // 已 accept 的连接 fd，负责 fd 生命周期和半关闭操作。
    Channel channel_;                    // 把连接 fd 的读、写、关闭、错误事件绑定到本对象回调。
    const InetAddress localAddr_;        // 本端地址，通常由 getsockname(connfd) 获取。
    const InetAddress peerAddr_;         // 对端地址，来自 accept 返回的客户端地址。
    Buffer inputBuffer_;                 // 输入缓冲区，只在所属 loop 线程内读入和消费。
    Buffer outputBuffer_;     
    std::any context_;

    MessageCallback messageCallback_;       // 读到数据后把连接和输入缓冲区交给用户。
    ConnectionCallback connectionCallback_; // 连接建立或断开时通知用户。
    CloseCallback closeCallback_;           // 连接关闭时通知 TcpServer 从连接表移除。

    // 空闲超时
    std::chrono::steady_clock::time_point lastActiveTime_{std::chrono::steady_clock::now()};
    std::chrono::seconds idleTimeout_{0};  // 0 表示不启用

    // 写入超时
    TimePoint writeStartTime_{};           // 数据开始积压的时间点
    Duration writeTimeout_{0};             // 0 表示不启用

    // 优雅关闭超时
    Duration shutdownTimeout_{std::chrono::seconds(5)};  // 默认 5 秒
    TimerId shutdownTimerId_;              // 用于在正常关闭时取消该定时器

    
};


}
