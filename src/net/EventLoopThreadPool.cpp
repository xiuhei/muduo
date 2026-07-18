#include "net/EventLoopThreadPool.h"
#include "base/Logger.h"
#include "net/EventLoopThread.h"

#include"net/EventLoop.h"

namespace muduo
{
EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseloop)
    : baseLoop_(baseloop) {
    baseLoop_->assertInLoopThread();
}

void EventLoopThreadPool::setThreadNum(int numThreads) {
    numThreads_ = numThreads;
}

void EventLoopThreadPool::start(){
    baseLoop_->assertInLoopThread();
    if (started_) {
        return;
    }
    started_ = true;
    LOG_INFO("starting EventLoopThreadPool with {} IO threads", numThreads_);

    // 每个 EventLoopThread 拥有一个独立 IO 线程和一个独立 EventLoop。
    for (int i = 0; i < numThreads_; ++i) {
        auto thread = std::make_unique<EventLoopThread>();
        EventLoop* loop = thread->startLoop();
        threads_.push_back(std::move(thread));
        loops_.push_back(loop);
    }
    LOG_INFO("EventLoopThreadPool started with {} IO threads", loops_.size());
}

EventLoop* EventLoopThreadPool::getNextLoop(){
    baseLoop_->assertInLoopThread();
    EventLoop* loop =baseLoop_;
    if(!loops_.empty()){
        loop=loops_[next_];
        ++next_;
        if (next_ == loops_.size()) {
            next_ = 0;
        }
    }
    return loop;
}
    

}// namespace muduo
