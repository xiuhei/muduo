#pragma once

#include "base/NonCopyable.h"
#include <string>
#include <netinet/in.h>


namespace muduo
{

// IPv4 sockaddr_in 的值类型封装，负责文本地址与系统地址结构之间的转换。
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
