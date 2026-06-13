#include "net/EventLoop.h"
#include "net/Poller.h"
#include "net/Channel.h"


#include <cerrno>
#include <stdexcept>
#include <sys/eventfd.h>
#include <unistd.h>
#include <cstring>
#include <memory>

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
        poller_->poll(10000, &activeChannels_);
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
    // eventfd 计数累加即可表达“有任务待处理”，不需要传递具体任务内容。
    ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
    (void)n;
}


}// namespace muduo
