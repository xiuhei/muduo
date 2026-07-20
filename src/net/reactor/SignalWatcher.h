#pragma once

#include "base/NonCopyable.h"
#include "net/reactor/Channel.h"

#include <functional>
#include <initializer_list>

namespace muduo {

// 通过 signalfd 把进程信号转换为普通 EventLoop 读事件。
// 必须在启动任何工作线程之前构造，使信号掩码被后续线程继承。
class SignalWatcher : public NonCopyable {
public:
    using SignalCallback = std::function<void(int)>;

    // 必须在创建日志线程或 IO 线程之前调用。
    static void block(std::initializer_list<int> signals);
    SignalWatcher(EventLoop* loop, std::initializer_list<int> signals);
    ~SignalWatcher();

    void setCallback(SignalCallback callback) { callback_ = std::move(callback); }

private:
    void handleRead();

    EventLoop* loop_;
    int signalFd_;
    Channel signalChannel_;
    SignalCallback callback_;
};

} // namespace muduo
