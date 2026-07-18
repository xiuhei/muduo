#include "net/Socket.h"
#include "net/InetAddress.h"
#include "base/Logger.h"

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace muduo
{

Socket::~Socket() {
    if (sockfd_ >= 0) {
        ::close(sockfd_);
    }
}

void Socket::bindAddress(const InetAddress& localaddr) {
    const struct sockaddr_in& addr = localaddr.getSockAddrIn();
    if (::bind(sockfd_, reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR("Socket::bindAddress: {}", std::strerror(errno));
    }
}

void Socket::listen() {
    if (::listen(sockfd_, SOMAXCONN) < 0) {
        LOG_ERROR("Socket::listen: {}", std::strerror(errno));
    }
}

int Socket::accept(InetAddress* peeraddr) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int connfd = ::accept4(sockfd_, reinterpret_cast<struct sockaddr*>(&addr), &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd >= 0) {
        *peeraddr = InetAddress(addr);
    }
    return connfd;
}

void Socket::shutdownWrite() {
    if (::shutdown(sockfd_, SHUT_WR) < 0) {
        LOG_ERROR("Socket::shutdownWrite: {}", std::strerror(errno));
    }
}

void Socket::setReuseAddr(bool on) {
    int optval = on ? 1 : 0;
    if (::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        LOG_ERROR("Socket::setReuseAddr: {}", std::strerror(errno));
    }
}




}// namespace muduo
