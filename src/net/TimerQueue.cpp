#include "net/TimerQueue.h"
#include "base/Logger.h"
#include "net/EventLoop.h"

#include <sys/timerfd.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <cerrno>

namespace muduo
{

namespace {

// 创建非阻塞、close-on-exec 的单调时钟 timerfd
int createTimerfd()
{
    int fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0) {
        LOG_CRITICAL("failed to create timerfd: {}", std::strerror(errno));
        throw std::runtime_error(std::string("timerfd_create: ") + std::strerror(errno));
    }
    return fd;
}

// 将 TimePoint 转换为 timerfd_settime 需要的 itimerspec
struct itimerspec timePointToItimerspec(TimePoint when)
{
    auto now = std::chrono::steady_clock::now();
    struct itimerspec ts{};

    if (when <= now) {
        // 已过期：立即触发
        ts.it_value.tv_sec = 0;
        ts.it_value.tv_nsec = 1; // 1ns，几乎立即
    } else {
        auto duration = when - now;
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(duration);
        auto nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - secs);
        ts.it_value.tv_sec = secs.count();
        ts.it_value.tv_nsec = nsecs.count();
    }
    // it_interval 为 0，表示不重复（timerfd 的重复由 TimerQueue 自行管理）
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;
    return ts;
}

} // namespace

// 构造：创建 timerfd，注册到 EventLoop 的 epoll 中
TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop)
    , timerfd_(createTimerfd())
    , timerfdChannel_(loop, timerfd_)
{
    timerfdChannel_.setReadCallback([this] { handleRead(); });
    timerfdChannel_.enableReading();
}

// 析构：关闭 timerfd channel，释放所有未触发的定时器
TimerQueue::~TimerQueue()
{
    timerfdChannel_.disableAll();
    loop_->removeChannel(&timerfdChannel_);
    ::close(timerfd_);
    // 清理所有未触发的定时器
    for (const auto& entry : timers_) {
        delete entry.second;
    }
}

// 添加定时器（线程安全）：构造 Timer 后投递给 EventLoop 线程处理
TimerId TimerQueue::addTimer(TimerCallback cb, TimePoint when, Duration interval)
{
    Timer* timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop([this, timer] { addTimerInLoop(timer); });
    return TimerId(timer, timer->sequence());
}

// 在 EventLoop 线程中插入定时器；若插在队首则更新 timerfd 到期时间
void TimerQueue::addTimerInLoop(Timer* timer)
{
    loop_->assertInLoopThread();
    bool earliestChanged = insert(timer);
    if (earliestChanged) {
        resetTimerfd(timer->expiration());
    }
}

// 取消定时器（线程安全）：通过 runInLoop 投递到 EventLoop 线程执行
void TimerQueue::cancel(TimerId timerId)
{
    loop_->runInLoop([this, timerId] { cancelInLoop(timerId); });
}

// 在 EventLoop 线程中取消定时器：从 activeTimers_ 和 timers_ 中移除并释放
void TimerQueue::cancelInLoop(TimerId timerId)
{
    loop_->assertInLoopThread();
    if (!timerId.timer_) return;

    auto it = activeTimers_.find(timerId.timer_);
    if (it == activeTimers_.end()) {
        // 定时器已经到期或被取消
        return;
    }
    activeTimers_.erase(it);

    // 从 timers_ 中删除
    auto range = timers_.equal_range(Entry(timerId.timer_->expiration(), timerId.timer_));
    for (auto rit = range.first; rit != range.second; ++rit) {
        if (rit->second == timerId.timer_) {
            timers_.erase(rit);
            break;
        }
    }
    delete timerId.timer_;
}

// 获取最近一个定时器的到期时间点，用于设置 poll 超时
std::optional<TimePoint> TimerQueue::getNextTimeout() const
{
    if (timers_.empty()) return std::nullopt;
    return timers_.begin()->first;
}

// timerfd 可读回调：读取事件并执行所有已到期的定时器回调
void TimerQueue::handleRead()
{
    loop_->assertInLoopThread();
    readTimerfd();

    TimePoint now = std::chrono::steady_clock::now();
    std::vector<Entry> expired = getExpired(now);

    callingExpiredTimers_ = true;
    for (const auto& entry : expired) {
        entry.second->run();
    }
    callingExpiredTimers_ = false;

    reset(expired, now);
}

// 取出所有到期时间 <= now 的定时器，从 timers_ 和 activeTimers_ 中移除
std::vector<TimerQueue::Entry> TimerQueue::getExpired(TimePoint now)
{
    std::vector<Entry> expired;

    // sentry 作为哨兵，now 时间点 + 最大指针，所有 <= now 的 entry 都会排在前面
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    auto end = timers_.lower_bound(sentry);

    expired.assign(timers_.begin(), end);

    // 从 timers_ 和 activeTimers_ 中移除已过期的定时器
    for (auto it = timers_.begin(); it != end; ++it) {
        activeTimers_.erase(it->second);
    }
    timers_.erase(timers_.begin(), end);

    return expired;
}

// 重置已到期定时器：重复的重新加入队列，一次性的释放内存；最后更新 timerfd
void TimerQueue::reset(const std::vector<Entry>& expired, TimePoint now)
{
    for (const auto& entry : expired) {
        Timer* timer = entry.second;
        if (timer->repeat()) {
            timer->restart(now);
            activeTimers_.insert(timer);
            timers_.insert(Entry(timer->expiration(), timer));
        } else {
            delete timer;
        }
    }

    // 重新设置 timerfd 到期时间
    if (!timers_.empty()) {
        resetTimerfd(timers_.begin()->first);
    }
}

// 将定时器插入 timers_ 和 activeTimers_，返回是否插在了队首位置
bool TimerQueue::insert(Timer* timer)
{
    loop_->assertInLoopThread();
    bool earliestChanged = false;

    if (timers_.empty() || timer->expiration() < timers_.begin()->first) {
        earliestChanged = true;
    }

    timers_.insert(Entry(timer->expiration(), timer));
    activeTimers_.insert(timer);

    return earliestChanged;
}

// 用 timerfd_settime 设置下一次到期时间
void TimerQueue::resetTimerfd(TimePoint expiration)
{
    struct itimerspec newValue = timePointToItimerspec(expiration);
    struct itimerspec oldValue{};
    if (::timerfd_settime(timerfd_, 0, &newValue, &oldValue) < 0) {
        LOG_ERROR("TimerQueue::resetTimerfd: {}", std::strerror(errno));
    }
}

// 读取 timerfd，消费内核中累积的到期次数
void TimerQueue::readTimerfd()
{
    uint64_t expirations = 0;
    ssize_t n = ::read(timerfd_, &expirations, sizeof(expirations));
    if (n < 0 && errno != EAGAIN) {
        LOG_ERROR("failed to read timerfd {}: {}", timerfd_, std::strerror(errno));
    }
}

} // namespace muduo
