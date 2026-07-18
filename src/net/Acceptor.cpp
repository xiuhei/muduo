#include "net/Acceptor.h"

#include "base/Logger.h"
#include "net/EventLoop.h"
#include "net/Socket.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace  muduo {

namespace {

int createNonblockingSocket() {
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0) {
        LOG_CRITICAL("failed to create accept socket: {}", std::strerror(errno));
        throw std::runtime_error(std::string("socket: ") + std::strerror(errno));
    }
    return sockfd;
}

} // namespace

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr)
    : loop_(loop),
      acceptSocket_(createNonblockingSocket()),
      acceptChannel_(loop, acceptSocket_.fd()) {
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallback([this] { handleRead(); });
}

Acceptor::~Acceptor() {
    stop();
}

void Acceptor::stop() {
    loop_->assertInLoopThread();
    if (!listening_) return;

    listening_ = false;
    acceptChannel_.disableAll();
    loop_->removeChannel(&acceptChannel_);
    LOG_INFO("acceptor stopped listening on fd={}", acceptSocket_.fd());
}

void Acceptor::listen() {
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
    LOG_INFO("acceptor started listening on fd={}", acceptSocket_.fd());
}

void Acceptor::handleRead() {
    // listen fd 也是非阻塞的，一次读事件尽量 accept 到 EAGAIN。
    while (true) {
        InetAddress peerAddr(0);
        int connfd = acceptSocket_.accept(&peerAddr);
        if (connfd >= 0) {
            if (listening_ && newConnectionCallback_) {
                newConnectionCallback_(connfd, peerAddr);
            } else {
                LOG_WARN("accepted fd={} from {} without a connection callback; closing it",
                         connfd, peerAddr.toIpPort());
                ::close(connfd);
            }
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        LOG_ERROR("accept failed on fd={}: {}", acceptSocket_.fd(), std::strerror(errno));
        break;
    }
}

} // namespace  muduo
