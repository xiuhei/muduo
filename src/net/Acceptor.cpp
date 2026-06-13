#include "net/Acceptor.h"

#include "net/EventLoop.h"
#include "net/Socket.h"

#include <cerrno>
#include <cstring>
#include <stdio.h>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace  muduo {

namespace {

int createNonblockingSocket() {
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0) {
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
    if (listening_) {
        acceptChannel_.disableAll();
        loop_->removeChannel(&acceptChannel_);
    }
}

void Acceptor::listen() {
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

void Acceptor::handleRead() {
    // listen fd 也是非阻塞的，一次读事件尽量 accept 到 EAGAIN。
    while (true) {
        InetAddress peerAddr(0);
        int connfd = acceptSocket_.accept(&peerAddr);
        if (connfd >= 0) {
            if (newConnectionCallback_) {
                newConnectionCallback_(connfd, peerAddr);
            } else {
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
        break;
    }
}

} // namespace  muduo
