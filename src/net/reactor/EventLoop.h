#pragma once

#include "base/NonCopyable.h"
#include "net/reactor/Channel.h"
#include "net/timer/Timer.h"
#include "net/timer/TimerId.h"

#include <atomic>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <functional>

namespace muduo
{

class Poller;
class TimerQueue;

// 单线程 Reactor：串行处理 I/O、定时器和跨线程投递任务。
// 除明确标注线程安全的接口外，Channel 操作必须由创建该对象的线程执行。
class EventLoop : public NonCopyable {
public:
    using Functor = std::function<void()>;
    using TimerCallback = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();
    void quit();
    bool isInLoopThread() const;
    void assertInLoopThread() const;
    // 当前已在 loop 线程时立即执行，否则入队并唤醒 loop。
    void runInLoop(Functor cb);
    // 始终入队，适合避免当前回调栈内重入。
    void queueInLoop(Functor cb);

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

    // 定时器相关接口
    // 在指定时刻执行回调
    TimerId runAt(TimePoint time, TimerCallback cb);
    // 延迟 delay 后执行一次
    TimerId runAfter(Duration delay, TimerCallback cb);
    // 每隔 interval 重复执行
    TimerId runEvery(Duration interval, TimerCallback cb);
    // 取消定时器
    void cancel(TimerId timerId);

private:
    // eventfd 把跨线程任务转换成普通可读事件。
    void handleWakeupRead();
    void doPendingFunctors();
    void wakeup();

    std::atomic<bool> looping_{false};
    std::atomic<bool> quit_{false};
    std::atomic<bool> callingPendingFunctors_{false};
    const std::thread::id threadId_{};
    std::unique_ptr<Poller> poller_;
    std::unique_ptr<TimerQueue> timerQueue_;
    std::vector<Channel*> activeChannels_;

    int wakeupFd_{};
    Channel wakeupChannel_;
    std::mutex mutex_{};
    std::vector<Functor> pendingFunctors_;

};

} // namespace muduo
