#pragma once

#include "base/NonCopyable.h"

#include <functional>

namespace muduo
{

class EventLoop;

class Channel : public NonCopyable {
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd) : fd_(fd), loop_(loop) {};
    ~Channel() = default;

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
    void update();

    int fd_{-1};
    EventLoop* loop_;
    int events_{0};
    int revents_{0};
    int index_{-1};


    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

};

} // namespace muduo
