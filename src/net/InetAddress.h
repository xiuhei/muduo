#pragma once

#include "base/NonCopyable.h"
#include <string>
#include <netinet/in.h>


namespace muduo
{

class InetAddress {

public:

    explicit InetAddress(uint16_t port, const std::string& ip="0.0.0.0");
    explicit InetAddress(const sockaddr_in& addr) : addr_(addr) {}

    const struct sockaddr_in& getSockAddrIn() const { return addr_; }
    std::string toIpPort() const;

private:
    struct sockaddr_in addr_{};

};
}// namespace muduo