#pragma once

#include "base/NonCopyable.h"

namespace muduo
{
class InetAddress;

// socket fd 的独占 RAII 封装；析构时关闭 fd。
class Socket : public NonCopyable {

public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    ~Socket();

    int fd() const { return sockfd_; }

    bool bindAddress(const InetAddress& localaddr);
    bool listen();
    // 返回带 NONBLOCK/CLOEXEC 的连接 fd；成功后所有权交给调用方。
    int accept(InetAddress* peeraddr);
    void shutdownWrite();


    void setReuseAddr(bool on) ;

private:
    int sockfd_{-1};

};

} // namespace muduo
