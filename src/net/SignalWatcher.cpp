#include "net/SignalWatcher.h"

#include "base/Logger.h"
#include "net/EventLoop.h"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <stdexcept>
#include <sys/signalfd.h>
#include <unistd.h>

namespace muduo {

namespace {

sigset_t makeSignalMask(std::initializer_list<int> signals) {
    sigset_t mask;
    ::sigemptyset(&mask);
    for (int signal : signals) ::sigaddset(&mask, signal);
    return mask;
}

int createSignalFd(const sigset_t& mask) {
    int fd = ::signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (fd < 0) {
        throw std::runtime_error(std::string("signalfd: ") + std::strerror(errno));
    }
    return fd;
}

} // namespace

void SignalWatcher::block(std::initializer_list<int> signals) {
    sigset_t mask = makeSignalMask(signals);
    int error = ::pthread_sigmask(SIG_BLOCK, &mask, nullptr);
    if (error != 0) {
        throw std::runtime_error(std::string("pthread_sigmask: ") + std::strerror(error));
    }
}

SignalWatcher::SignalWatcher(EventLoop* loop, std::initializer_list<int> signals)
    : loop_(loop),
      signalFd_((block(signals), createSignalFd(makeSignalMask(signals)))),
      signalChannel_(loop, signalFd_) {
    loop_->assertInLoopThread();
    signalChannel_.setReadCallback([this] { handleRead(); });
    signalChannel_.enableReading();
}

SignalWatcher::~SignalWatcher() {
    loop_->assertInLoopThread();
    signalChannel_.disableAll();
    loop_->removeChannel(&signalChannel_);
    ::close(signalFd_);
}

void SignalWatcher::handleRead() {
    signalfd_siginfo info{};
    while (true) {
        ssize_t n = ::read(signalFd_, &info, sizeof(info));
        if (n == static_cast<ssize_t>(sizeof(info))) {
            LOG_INFO("received signal {}", info.ssi_signo);
            if (callback_) callback_(static_cast<int>(info.ssi_signo));
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        if (n < 0) LOG_ERROR("signalfd read failed: {}", std::strerror(errno));
        break;
    }
}

} // namespace muduo
