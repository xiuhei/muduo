#pragma once

#include "base/NonCopyable.h"

namespace muduo
{
class InetAddress;

class Socket : public NonCopyable {

public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    ~Socket();

    int fd() const { return sockfd_; }

    void bindAddress(const InetAddress& localaddr);
    void listen();
    int accept( InetAddress* peeraddr);
    void shutdownWrite();


    void setReuseAddr(bool on) ;

private:
    int sockfd_{-1};

};

} // namespace muduo