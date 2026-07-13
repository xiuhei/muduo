#pragma once

#include <cstdint>

namespace muduo
{

class Timer;

// 定时器标识符，用于取消定时器
// 包含 Timer 指针和序列号，避免悬空指针问题
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
