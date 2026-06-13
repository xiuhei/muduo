#pragma once

#include "base/NonCopyable.h"

#include <condition_variable>
#include <mutex>
#include <thread>
namespace muduo
{
class EventLoop;

class EventLoopThread : public NonCopyable {
public:
    EventLoopThread();
    ~EventLoopThread();


    // 启动线程并阻塞等待 EventLoop 创建完成，返回的指针在线程退出前有效。
    EventLoop* startLoop();
private:
    void threadFunc();

    bool exiting_{false};
    EventLoop* loop_{nullptr};
    std::thread thread_;    
    std::mutex mutex_;
    std::condition_variable cond_;
};


}// namespace muduo
