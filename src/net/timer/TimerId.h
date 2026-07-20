#pragma once

#include <cstdint>

namespace muduo
{

class Timer;

// 不拥有 Timer 的取消句柄，只应传回创建它的 TimerQueue/EventLoop。
// 当前队列以指针检查存活性，序列号保留给更严格的实例校验。
class TimerId
{
public:
    TimerId()
        : timer_(nullptr)
        , sequence_(0)
    {
    }

    TimerId(Timer* timer, int64_t seq)
        : timer_(timer)
        , sequence_(seq)
    {
    }

    // TimerQueue 需要访问内部成员
    friend class TimerQueue;

private:
    Timer* timer_;
    int64_t sequence_{0};
};

} // namespace muduo
