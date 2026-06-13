#include "net/Channel.h"
#include "net/EventLoop.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/epoll.h>

namespace muduo{

/*
事件分发函数，调用对应的回调函数处理事件。
EPOLLHUP: 连接被对方关闭，且没有数据可读。
EPOLLIN: 可读事件，表示有数据可读。
EPOLLPRI: 紧急数据可读事件。    
EPOLLRDHUP: 连接被对方关闭，且有数据可读。
EPOLLOUT: 可写事件，表示可以发送数据。
EPOLLERR: 错误事件，表示发生错误。
*/
void Channel::handleEvent() {
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (closeCallback_) closeCallback_();
    }
    if (revents_ & EPOLLERR) {
        if (errorCallback_) errorCallback_();
    }
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (readCallback_) readCallback_();
    }
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) writeCallback_();
    }
}

void Channel::enableReading() {
    events_ |= EPOLLIN;
    update();
}

void Channel::enableWriting() {
    events_ |= EPOLLOUT;
    update();
}

void Channel::disableWriting() {
    events_ &= ~EPOLLOUT;
    update();
}

void Channel::disableAll() {
    events_ = 0;
    update();
}

bool Channel::isWriting() const {
    return events_ & EPOLLOUT;
}

void Channel::update() {
    if (loop_) {
        loop_->updateChannel(this);
    } else {
        fprintf(stderr, "Channel::update() - no owner loop\n");
    }
}

} // namespace muduo