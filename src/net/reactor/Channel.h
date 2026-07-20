#pragma once

#include "base/NonCopyable.h"

#include <functional>

namespace muduo
{

class EventLoop;

// fd 的事件适配器：保存关注事件，并把 epoll 返回事件分派给上层回调。
// Channel 不拥有 fd，其生命周期必须覆盖在 Poller 中注册的时间。
class Channel : public NonCopyable {
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd) : fd_(fd), loop_(loop) {};
    ~Channel() = default;

    // 按关闭、错误、可读、可写的顺序分派本轮 revents。
    void handleEvent();

    int fd() const { return fd_; }
    int events() const { return events_; }
    void setRevents(int revents) { revents_ = revents; }
    bool isNoneEvent() const { return events_ == 0; }
    int index() const { return index_; }
    void setIndex(int index) { index_ = index; }
    EventLoop* ownerLoop() const { return loop_; }

    void setReadCallback(EventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb);}
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    void enableReading();
    void enableWriting();
    void disableWriting();
    void disableAll();
    bool isWriting() const ; 
private:
    // 将关注事件变化同步到所属 EventLoop/Poller。
    void update();

    int fd_{-1};
    EventLoop* loop_;
    int events_{0};
    int revents_{0};
    int index_{-1}; // Poller 内部注册状态：new/added/deleted。


    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

};

} // namespace muduo
