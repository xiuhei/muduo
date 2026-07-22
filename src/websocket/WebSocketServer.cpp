#include "websocket/WebSocketServer.h"

#include "base/Logger.h"
#include "net/tcp/Buffer.h"
#include "net/tcp/TcpConnection.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <limits>
#include <sstream>

namespace muduo {
namespace {

// 对 32 位整数循环左移，是 SHA-1 每轮运算使用的基本操作。
uint32_t rotateLeft(uint32_t value, unsigned bits) {
    return (value << bits) | (value >> (32 - bits));
}

// 计算输入的 SHA-1 摘要，供 WebSocket 握手生成 Sec-WebSocket-Accept。
// 实现放在协议内部，避免仅为握手引入额外加密库依赖。
std::array<unsigned char, 20> sha1(const std::string& input) {
    std::vector<unsigned char> data(input.begin(), input.end());
    const uint64_t bitLength = static_cast<uint64_t>(data.size()) * 8;
    // SHA-1 填充：追加 1 bit、补零到块尾预留 64 bit，再写入原始位长度。
    data.push_back(0x80);
    while ((data.size() % 64) != 56) data.push_back(0);
    for (int shift = 56; shift >= 0; shift -= 8) {
        data.push_back(static_cast<unsigned char>(bitLength >> shift));
    }

    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476, h4 = 0xC3D2E1F0;
    // 每个 512 bit 数据块扩展为 80 个字，并执行 80 轮压缩运算。
    for (size_t offset = 0; offset < data.size(); offset += 64) {
        uint32_t words[80]{};
        for (size_t i = 0; i < 16; ++i) {
            const size_t p = offset + i * 4;
            words[i] = (static_cast<uint32_t>(data[p]) << 24) |
                       (static_cast<uint32_t>(data[p + 1]) << 16) |
                       (static_cast<uint32_t>(data[p + 2]) << 8) | data[p + 3];
        }
        for (size_t i = 16; i < 80; ++i) {
            words[i] = rotateLeft(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^
                                      words[i - 16],
                                  1);
        }
        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (size_t i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            const uint32_t temp = rotateLeft(a, 5) + f + e + k + words[i];
            e = d; d = c; c = rotateLeft(b, 30); b = a; a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    std::array<unsigned char, 20> digest{};
    const uint32_t hashes[] = {h0, h1, h2, h3, h4};
    for (size_t i = 0; i < 5; ++i) {
        digest[i * 4] = static_cast<unsigned char>(hashes[i] >> 24);
        digest[i * 4 + 1] = static_cast<unsigned char>(hashes[i] >> 16);
        digest[i * 4 + 2] = static_cast<unsigned char>(hashes[i] >> 8);
        digest[i * 4 + 3] = static_cast<unsigned char>(hashes[i]);
    }
    return digest;
}

// 将二进制数据编码为 Base64，用于输出握手响应中的摘要文本。
std::string base64(const unsigned char* data, size_t length) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((length + 2) / 3) * 4);
    for (size_t i = 0; i < length; i += 3) {
        const uint32_t value = (static_cast<uint32_t>(data[i]) << 16) |
            (i + 1 < length ? static_cast<uint32_t>(data[i + 1]) << 8 : 0) |
            (i + 2 < length ? data[i + 2] : 0);
        result.push_back(alphabet[(value >> 18) & 63]);
        result.push_back(alphabet[(value >> 12) & 63]);
        result.push_back(i + 1 < length ? alphabet[(value >> 6) & 63] : '=');
        result.push_back(i + 2 < length ? alphabet[value & 63] : '=');
    }
    return result;
}

// 在逗号分隔的 HTTP 头字段中，不区分大小写地查找完整 token。
bool containsToken(const std::string& value, const std::string& expected) {
    size_t begin = 0;
    while (begin <= value.size()) {
        const size_t comma = value.find(',', begin);
        size_t end = comma == std::string::npos ? value.size() : comma;
        while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
        if (end - begin == expected.size() &&
            std::equal(value.begin() + begin, value.begin() + end, expected.begin(),
                       [](unsigned char a, unsigned char b) {
                           return std::tolower(a) == std::tolower(b);
                       })) return true;
        if (comma == std::string::npos) break;
        begin = comma + 1;
    }
    return false;
}

} // namespace

// 创建底层 TCP 服务，并将连接和数据事件转交给 WebSocket 协议处理函数。
WebSocketServer::WebSocketServer(EventLoop* loop, const InetAddress& listenAddr)
    : server_(loop, listenAddr, "WebSocketServer") {
    server_.setConnectionCallback(
        [this](const TcpConnectionPtr& conn) { onConnection(conn); });
    server_.setMessageCallback(
        [this](const TcpConnectionPtr& conn, Buffer* buf) { onMessage(conn, buf); });
}

// 为新连接初始化协议状态；连接断开时通知已经完成升级的 WebSocket 用户。
void WebSocketServer::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        conn->setContext_(ConnectionContext{});
        return;
    }
    const auto* context = std::any_cast<ConnectionContext>(&conn->getContext());
    if (context && context->upgraded && closeCallback_) closeCallback_(conn);
}

// 根据连接状态先处理 HTTP Upgrade，升级完成后再解析 WebSocket 帧。
void WebSocketServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf) {
    auto* context = std::any_cast<ConnectionContext>(conn->getMutableContext());
    if (!context) {
        conn->forceClose();
        return;
    }
    if (!context->upgraded && !handleUpgrade(conn, buf, context)) return;
    if (context->upgraded) parseFrames(conn, buf, context);
}

// 解析并校验 WebSocket 握手请求，成功时返回 101 并切换连接协议状态。
bool WebSocketServer::handleUpgrade(const TcpConnectionPtr& conn, Buffer* buf,
                                    ConnectionContext* context) {
    if (!context->http.parseRequest(buf)) {
        conn->send("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
        conn->shutdown();
        return false;
    }
    // TCP 数据可能分多次到达，HTTP 头未收完整时保留上下文并等待后续数据。
    if (!context->http.gotAll()) return false;

    const HttpRequest& request = context->http.request();
    const std::string upgrade = request.getHeader("Upgrade");
    const std::string connection = request.getHeader("Connection");
    const std::string key = request.getHeader("Sec-WebSocket-Key");
    const std::string version = request.getHeader("Sec-WebSocket-Version");

    // 没有 Upgrade 的普通 HTTP 请求可交给业务层，用于在同一端口提供测试页面。
    // 响应后关闭连接，因为此连接的解析上下文不会再复用于 HTTP keep-alive。
    if (upgrade.empty() && httpCallback_) {
        HttpResponse response;
        response.setCloseConnection(true);
        httpCallback_(request, &response);
        Buffer output;
        response.appendToBuffer(&output);
        conn->send(output.retrieveAllAsString());
        conn->shutdown();
        return false;
    }

    // WebSocket 请求必须完整满足 RFC 6455 的 Upgrade 条件。
    if (request.method() != HttpRequest::GET ||
        request.version() != HttpRequest::Version::Http11 ||
        !containsToken(upgrade, "websocket") ||
        !containsToken(connection, "upgrade") || key.empty() || version != "13") {
        conn->send("HTTP/1.1 426 Upgrade Required\r\nSec-WebSocket-Version: 13\r\n"
                   "Connection: close\r\nContent-Length: 0\r\n\r\n");
        conn->shutdown();
        return false;
    }

    // 将客户端 key 与 RFC 6455 固定 GUID 拼接，摘要并编码以证明服务端理解协议。
    const auto digest = sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    const std::string accept = base64(digest.data(), digest.size());
    conn->send("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
               "Connection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n\r\n");
    context->upgraded = true;
    if (openCallback_) openCallback_(conn, request);
    return true;
}

// 从接收缓冲区连续解析完整帧，处理控制帧、消息分片及应用层回调。
bool WebSocketServer::parseFrames(const TcpConnectionPtr& conn, Buffer* buf,
                                  ConnectionContext* context) {
    while (buf->readableBytes() >= 2) {
        const auto* bytes = reinterpret_cast<const unsigned char*>(buf->peek());
        const bool fin = (bytes[0] & 0x80) != 0;
        const bool masked = (bytes[1] & 0x80) != 0;
        const uint8_t rawOpcode = bytes[0] & 0x0F;
        uint64_t payloadLength = bytes[1] & 0x7F;
        size_t headerLength = 2;

        // 未协商扩展时 RSV 位必须为零；RFC 6455 要求客户端发来的帧必须掩码。
        if ((bytes[0] & 0x70) != 0 || !masked) {
            protocolError(conn, context, 1002, "invalid frame flags");
            return false;
        }
        // 长度标记 126/127 表示真实长度位于后续 2/8 字节，采用网络字节序。
        if (payloadLength == 126) {
            if (buf->readableBytes() < 4) return true;
            payloadLength = (static_cast<uint64_t>(bytes[2]) << 8) | bytes[3];
            headerLength = 4;
            if (payloadLength < 126) {
                protocolError(conn, context, 1002, "non-minimal length"); return false;
            }
        } else if (payloadLength == 127) {
            if (buf->readableBytes() < 10) return true;
            if ((bytes[2] & 0x80) != 0) {
                protocolError(conn, context, 1002, "invalid length"); return false;
            }
            payloadLength = 0;
            for (size_t i = 2; i < 10; ++i) payloadLength = (payloadLength << 8) | bytes[i];
            headerLength = 10;
            if (payloadLength <= 65535) {
                protocolError(conn, context, 1002, "non-minimal length"); return false;
            }
        }

        // 控制帧不可分片且载荷最多 125 字节，同时拒绝 RFC 保留的 opcode。
        const bool control = rawOpcode >= 0x8;
        if ((control && (!fin || payloadLength > 125)) ||
            rawOpcode == 0x3 || rawOpcode == 0x4 || rawOpcode == 0x5 ||
            rawOpcode == 0x6 || rawOpcode == 0x7 || rawOpcode > 0xA) {
            protocolError(conn, context, 1002, "invalid opcode"); return false;
        }
        if (payloadLength > maxMessageSize_ ||
            payloadLength > std::numeric_limits<size_t>::max() - headerLength - 4) {
            protocolError(conn, context, 1009, "message too large"); return false;
        }
        const size_t frameLength = headerLength + 4 + static_cast<size_t>(payloadLength);
        if (buf->readableBytes() < frameLength) return true;

        // 客户端载荷按四字节掩码循环异或；服务端发送帧则不使用掩码。
        const unsigned char* mask = bytes + headerLength;
        const unsigned char* encoded = mask + 4;
        std::string payload(static_cast<size_t>(payloadLength), '\0');
        for (size_t i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<char>(encoded[i] ^ mask[i % 4]);
        }
        buf->retrieve(frameLength);
        const Opcode opcode = static_cast<Opcode>(rawOpcode);

        // Ping 必须回 Pong；Pong 无需上报；Close 需要回送 Close 完成关闭握手。
        if (opcode == Opcode::Ping) {
            conn->send(makeFrame(Opcode::Pong, payload));
        } else if (opcode == Opcode::Pong) {
            continue;
        } else if (opcode == Opcode::Close) {
            if (payload.size() == 1) {
                protocolError(conn, context, 1002, "invalid close payload"); return false;
            }
            if (!context->closeSent) {
                context->closeSent = true;
                conn->send(makeFrame(Opcode::Close, payload));
            }
            conn->shutdown();
            return false;
        } else if (opcode == Opcode::Continuation) {
            // Continuation 只能跟在未结束的数据帧后，FIN 到达后才交付完整消息。
            if (context->fragmentedOpcode == Opcode::Continuation) {
                protocolError(conn, context, 1002, "unexpected continuation"); return false;
            }
            if (context->fragmentedPayload.size() > maxMessageSize_ - payload.size()) {
                protocolError(conn, context, 1009, "message too large"); return false;
            }
            context->fragmentedPayload += payload;
            if (fin) {
                if (messageCallback_)
                    messageCallback_(conn, context->fragmentedPayload,
                                     context->fragmentedOpcode);
                context->fragmentedPayload.clear();
                context->fragmentedOpcode = Opcode::Continuation;
            }
        } else {
            // 新数据帧不能与尚未完成的分片消息交错。
            if (context->fragmentedOpcode != Opcode::Continuation) {
                protocolError(conn, context, 1002, "interleaved message"); return false;
            }
            if (fin) {
                if (messageCallback_) messageCallback_(conn, payload, opcode);
            } else {
                context->fragmentedOpcode = opcode;
                context->fragmentedPayload = std::move(payload);
            }
        }
    }
    return true;
}

// 记录协议错误，发送带状态码和简短原因的 Close 帧，然后关闭连接。
void WebSocketServer::protocolError(const TcpConnectionPtr& conn,
                                    ConnectionContext* context, uint16_t code,
                                    const std::string& reason) {
    LOG_WARN("WebSocket protocol error from {}: {}", conn->peerAddress().toIpPort(), reason);
    if (!context->closeSent) {
        context->closeSent = true;
        std::string payload;
        payload.push_back(static_cast<char>(code >> 8));
        payload.push_back(static_cast<char>(code));
        payload += reason.substr(0, 123);
        conn->send(makeFrame(Opcode::Close, payload));
    }
    conn->shutdown();
}

// 按 RFC 6455 编码一个服务端帧，并根据载荷大小选择长度字段格式。
std::string WebSocketServer::makeFrame(Opcode opcode, const std::string& payload, bool fin) {
    std::string frame;
    frame.reserve(payload.size() + 10);
    frame.push_back(static_cast<char>((fin ? 0x80 : 0) | static_cast<uint8_t>(opcode)));
    // 服务端帧不掩码，所以第二字节只写长度标记，不设置 MASK 位。
    if (payload.size() <= 125) {
        frame.push_back(static_cast<char>(payload.size()));
    } else if (payload.size() <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<char>(payload.size() >> 8));
        frame.push_back(static_cast<char>(payload.size()));
    } else {
        frame.push_back(127);
        const uint64_t size = payload.size();
        for (int shift = 56; shift >= 0; shift -= 8)
            frame.push_back(static_cast<char>(size >> shift));
    }
    frame += payload;
    return frame;
}

// 将字符串封装为单个完整文本帧并发送。
void WebSocketServer::sendText(const TcpConnectionPtr& conn, const std::string& message) {
    conn->send(makeFrame(Opcode::Text, message));
}

// 将字节串封装为单个完整二进制帧并发送。
void WebSocketServer::sendBinary(const TcpConnectionPtr& conn, const std::string& message) {
    conn->send(makeFrame(Opcode::Binary, message));
}

// 主动发送 Close 帧并停止写入；关闭原因被限制为控制帧允许的最大长度。
void WebSocketServer::close(const TcpConnectionPtr& conn, uint16_t code,
                            const std::string& reason) {
    std::string payload;
    payload.push_back(static_cast<char>(code >> 8));
    payload.push_back(static_cast<char>(code));
    payload += reason.substr(0, 123);
    conn->send(makeFrame(Opcode::Close, payload));
    conn->shutdown();
}

} // namespace muduo
