#pragma once

#include "base/NonCopyable.h"
#include "net/reactor/Channel.h"
#include "net/tcp/InetAddress.h"
#include "net/tcp/Socket.h"

#include <functional>

namespace  muduo {

class EventLoop;

// 监听 socket 与 Reactor 的边界：接收新 fd 后把所有权交给 TcpServer。
class Acceptor : public NonCopyable {
public:
    using NewConnectionCallback = std::function<void(int, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr);
    ~Acceptor();

    void setNewConnectionCallback(NewConnectionCallback cb) { newConnectionCallback_ = std::move(cb); }
    void listen();
    void stop();
    bool listening() const { return listening_; }

private:
    // LT 模式下一直 accept 到 EAGAIN，避免遗留就绪连接。
    void handleRead();

    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_{false};
};

} // namespace  muduo
