#include "net/EventLoopThread.h"

#include "net/EventLoop.h"

namespace muduo {

EventLoopThread::EventLoopThread() = default;

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if (loop_) {
        // 析构可能发生在非 IO 线程，quit 会通过 eventfd 唤醒 loop。
        loop_->quit();
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop() {
    thread_ = std::thread([this] { threadFunc(); });

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 等待子线程中的 EventLoop 构造完毕，避免返回一个还不可用的指针。
        cond_.wait(lock, [this] { return loop_ != nullptr; });
        loop = loop_;
    }
    return loop;
}

void EventLoopThread::threadFunc() {
    // EventLoop 必须在线程函数内部构造，这样它记录的 threadId 才是 IO 线程。
    EventLoop loop;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }
    loop.loop();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // loop.loop() 退出后栈对象即将销毁，清空指针避免悬空。
        loop_ = nullptr;
    }
}

} // namespace muduo
