#pragma once

#include "base/NonCopyable.h"
#include "net/EventLoopThread.h"

#include <vector>
#include <memory>

namespace muduo
{
class EventLoop;

class EventLoopThreadPool : public NonCopyable {
public:
    explicit EventLoopThreadPool(EventLoop* baseloop);

    void start();

    void setThreadNum(int numThreads);
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
