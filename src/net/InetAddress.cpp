#include "net/InetAddress.h"
#include "base/Logger.h"
#include <arpa/inet.h>

namespace muduo
{

InetAddress::InetAddress(uint16_t port, const std::string& ip) {
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr) <= 0) {
        LOG_ERROR("InetAddress::InetAddress: {}", std::strerror(errno));
    }
}

std::string InetAddress::toIpPort() const {
    char buf[64]{};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return std::string(buf) + ":" + std::to_string(ntohs(addr_.sin_port));
}
}
