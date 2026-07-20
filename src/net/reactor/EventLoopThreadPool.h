#pragma once

#include "base/NonCopyable.h"
#include "net/reactor/EventLoopThread.h"

#include <vector>
#include <memory>

namespace muduo
{
class EventLoop;

// IO EventLoop 线程池；baseLoop 负责接入连接，子 loop 轮询承载连接 I/O。
class EventLoopThreadPool : public NonCopyable {
public:
    explicit EventLoopThreadPool(EventLoop* baseloop);

    void start();

    void setThreadNum(int numThreads);
    // 仅在 baseLoop 线程调用；无子线程时退化为返回 baseLoop。
    EventLoop* getNextLoop();
private:
    EventLoop* baseLoop_;
    bool started_{false};
    int numThreads_{0};
    size_t next_{0};
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};    

}// namespace muduo
