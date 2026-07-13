#include "net/EventLoop.h"
#include "net/Poller.h"
#include "net/Channel.h"
#include "net/TimerQueue.h"

#include <cerrno>
#include <stdexcept>
#include <sys/eventfd.h>
#include <unistd.h>
#include <cstring>
#include <memory>
#include <chrono>
#include <optional>

namespace muduo
{

namespace {
int createEventFd() {
    int fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (fd < 0) {
        throw std::runtime_error(std::string("eventfd: ") + std::strerror(errno));
    }
    return fd;
}
} // namespace

EventLoop::EventLoop()
    :  threadId_(std::this_thread::get_id()),
      poller_(std::make_unique<Poller>()),
      timerQueue_(std::make_unique<TimerQueue>(this)),
      wakeupFd_(createEventFd()),
      wakeupChannel_(this,wakeupFd_) {
        wakeupChannel_.setReadCallback([this]{handleWakeupRead(); });
        wakeupChannel_.enableReading();
      }

EventLoop::~EventLoop() {
    wakeupChannel_.disableAll();
    removeChannel(&wakeupChannel_);
    ::close(wakeupFd_);
}

void EventLoop::loop() {
    assertInLoopThread();
    looping_ = true;
    quit_ = false;
    while (!quit_) {
        activeChannels_.clear();

        // 使用最近定时器的到期时间作为 poll 超时，避免定时器延迟触发
        auto nextTimeout = timerQueue_->getNextTimeout();
        int timeoutMs = 10000;  // 默认 10s
        if (nextTimeout.has_value()) {
            auto now = std::chrono::steady_clock::now();
            if (nextTimeout.value() <= now) {
                timeoutMs = 0;  // 已有定时器到期，立即返回
            } else {
                auto duration = nextTimeout.value() - now;
                timeoutMs = static_cast<int>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
                if (timeoutMs < 1) timeoutMs = 1;  // 至少 1ms
            }
        }

        poller_->poll(timeoutMs, &activeChannels_);
        for (Channel* channel : activeChannels_) {
            channel->handleEvent();
        }
        // IO 事件处理完以后，再执行跨线程投递过来的任务。
        doPendingFunctors();
    }
    looping_ = false;
}

bool EventLoop::isInLoopThread() const{
    return threadId_==std::this_thread::get_id();
}

void EventLoop::assertInLoopThread() const {
    if (!isInLoopThread()) {
        throw std::logic_error("EventLoop method called from the wrong thread");
    }
}

void EventLoop::quit() {
    quit_ = true;
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::updateChannel(Channel* channel) {
    assertInLoopThread();
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
    assertInLoopThread();
    poller_->removeChannel(channel);
}

void EventLoop::handleWakeupRead(){
    uint64_t one = 0;
    // 读掉 eventfd 计数，避免唤醒事件一直处于可读状态。
    ssize_t n = ::read(wakeupFd_, &one, sizeof(one));
    (void)n;
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // 缩短持锁时间：先交换到局部变量，再逐个执行回调。
        functors.swap(pendingFunctors_);
    }
    for (const auto& functor : functors) {
        functor();
    }
    callingPendingFunctors_ = false;
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        // 已经在所属线程中，立即执行可避免不必要的 eventfd 唤醒。
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    // eventfd 计数累加即可表达”有任务待处理”，不需要传递具体任务内容。
    ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
    (void)n;
}

TimerId EventLoop::runAt(TimePoint time, TimerCallback cb)
{
    return timerQueue_->addTimer(std::move(cb), time, Duration::zero());
}

TimerId EventLoop::runAfter(Duration delay, TimerCallback cb)
{
    TimePoint when = std::chrono::steady_clock::now() + delay;
    return timerQueue_->addTimer(std::move(cb), when, Duration::zero());
}

TimerId EventLoop::runEvery(Duration interval, TimerCallback cb)
{
    TimePoint when = std::chrono::steady_clock::now() + interval;
    return timerQueue_->addTimer(std::move(cb), when, interval);
}

void EventLoop::cancel(TimerId timerId)
{
    timerQueue_->cancel(timerId);
}

}// namespace muduo
