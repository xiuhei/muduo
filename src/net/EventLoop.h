#pragma once

#include "base/NonCopyable.h"
#include "net/Channel.h"

#include <atomic>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <functional>

namespace muduo
{


class Poller;

    
class EventLoop : public NonCopyable {
public:
    using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();

    void loop();
    void quit();
    bool isInLoopThread() const;
    void assertInLoopThread() const;
    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

private:
    void handleWakeupRead();
    void doPendingFunctors();
    void wakeup();

    std::atomic<bool> looping_{false};
    std::atomic<bool> quit_{false};
    std::atomic<bool> callingPendingFunctors_{false};
    const std::thread::id threadId_{};
    std::unique_ptr<Poller> poller_;
    std::vector<Channel*> activeChannels_;

    int wakeupFd_{};
    Channel wakeupChannel_;
    std::mutex mutex_{};
    std::vector<Functor> pendingFunctors_;


};

} // namespace muduo
