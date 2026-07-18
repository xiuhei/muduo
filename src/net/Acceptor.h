#pragma once

#include "base/NonCopyable.h"
#include "net/Channel.h"
#include "net/InetAddress.h"
#include "net/Socket.h"

#include <functional>

namespace  muduo {

class EventLoop;

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
    void handleRead();

    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_{false};
};

} // namespace  muduo
