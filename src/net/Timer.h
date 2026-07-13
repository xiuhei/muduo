#pragma once

#include "base/NonCopyable.h"

#include <functional>
#include <chrono>
#include <atomic>

namespace muduo
{

using Duration   = std::chrono::microseconds;
using TimePoint  = std::chrono::steady_clock::time_point;
using Seconds    = std::chrono::seconds;
using Milliseconds = std::chrono::milliseconds;

// 定时器类，表示一个可重复或一次性的定时任务
class Timer : public NonCopyable
{
public:
    using TimerCallback = std::function<void()>;

    // cb: 定时器到期时执行的回调
    // when: 到期时间点
    // interval: 重复间隔，0 表示一次性定时器
    Timer(TimerCallback cb, TimePoint when, Duration interval)
        : callback_(std::move(cb))
        , expiration_(when)
        , interval_(interval)
        , repeat_(interval > Duration::zero())
        , sequence_(++s_numCreated_)
    {
    }

    void run() const
    {
        if (callback_) callback_();
    }

    TimePoint expiration() const { return expiration_; }
    bool repeat() const { return repeat_; }
    Duration interval() const { return interval_; }
    int64_t sequence() const { return sequence_; }

    // 重新设置定时器（用于重复定时器）
    void restart(TimePoint now)
    {
        if (repeat_) {
            expiration_ = now + interval_;
        } else {
            expiration_ = TimePoint{};
        }
    }

    static int64_t numCreated() { return s_numCreated_; }

private:
    const TimerCallback callback_;
    TimePoint expiration_;
    const Duration interval_;
    const bool repeat_;
    const int64_t sequence_{0};

    inline static std::atomic<int64_t> s_numCreated_{0};
};

} // namespace muduo
