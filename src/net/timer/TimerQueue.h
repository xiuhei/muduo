#pragma once

#include "base/NonCopyable.h"
#include "net/reactor/Channel.h"
#include "net/timer/Timer.h"
#include "net/timer/TimerId.h"

#include <set>
#include <vector>
#include <optional>
#include <atomic>

namespace muduo
{

class EventLoop;

// Reactor 与定时器模块的边界：用 timerfd 把最近到期时间转换为 I/O 事件。
// 内部状态仅在所属 loop 线程访问；addTimer/cancel 通过投递保证线程安全。
class TimerQueue : public NonCopyable
{
public:
    using TimerCallback = std::function<void()>;

    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    // 添加定时器，线程安全（可在其他线程调用）
    // cb: 到期回调
    // when: 到期时间点
    // interval: 重复间隔，零间隔表示一次性定时器
    TimerId addTimer(TimerCallback cb, TimePoint when, Duration interval);

    // 取消定时器，线程安全（可在其他线程调用）
    void cancel(TimerId timerId);

    // 获取最近一个定时器的到期时间，用于设置 poll 超时
    // 如果没有定时器，返回 std::nullopt
    std::optional<TimePoint> getNextTimeout() const;

private:
    // Entry 用于在 set 中按到期时间排序
    // pair<到期时间, Timer*>，std::pair 自带的 lexicographical 比较正好满足需求
    using Entry = std::pair<TimePoint, Timer*>;
    using TimerSet = std::set<Entry>;

    // 以下方法必须在 EventLoop 线程中调用
    void addTimerInLoop(Timer* timer);
    void cancelInLoop(TimerId timerId);

    // timerfd 可读时的回调
    void handleRead();

    // 获取所有已过期的定时器
    std::vector<Entry> getExpired(TimePoint now);

    // 重置定时器（将重复定时器重新加入队列，并更新 timerfd）
    void reset(const std::vector<Entry>& expired, TimePoint now);

    // 将定时器插入 set，返回是否插在了队首（最早到期）
    bool insert(Timer* timer);

    // 用 timespec 设置 timerfd 的到期时间
    void resetTimerfd(TimePoint expiration);

    // 读取 timerfd，消费到期事件
    void readTimerfd();

    EventLoop* loop_;
    const int timerfd_;
    Channel timerfdChannel_;
    TimerSet timers_;
    std::set<Timer*> activeTimers_;   // 用于 cancel 时快速检查 timer 是否仍存活
    std::atomic<bool> callingExpiredTimers_{false};
};

} // namespace muduo
