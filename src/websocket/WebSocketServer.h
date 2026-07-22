#pragma once

#include "http/HttpContext.h"
#include "http/HttpResponse.h"
#include "net/tcp/TcpServer.h"

#include <cstdint>
#include <functional>
#include <string>

namespace muduo {

// RFC 6455 WebSocket server。连接首先按 HTTP/1.1 解析，Upgrade 成功后切换为帧协议。
class WebSocketServer {
public:
    enum class Opcode : uint8_t {
        Continuation = 0x0,
        Text = 0x1,
        Binary = 0x2,
        Close = 0x8,
        Ping = 0x9,
        Pong = 0xA,
    };

    using OpenCallback = std::function<void(const TcpConnectionPtr&, const HttpRequest&)>;
    using MessageCallback =
        std::function<void(const TcpConnectionPtr&, const std::string&, Opcode)>;
    using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
    using HttpCallback = std::function<void(const HttpRequest&, HttpResponse*)>;
    using TimerCallback = std::function<void()>;

    WebSocketServer(EventLoop* loop, const InetAddress& listenAddr);

    void setThreadNum(int num) { server_.setThreadNum(num); }
    void setOpenCallback(OpenCallback cb) { openCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }
    // 可选：处理未请求 WebSocket Upgrade 的普通 HTTP 请求，适合提供测试页面。
    void setHttpCallback(HttpCallback cb) { httpCallback_ = std::move(cb); }
    void setMaxMessageSize(size_t bytes) { maxMessageSize_ = bytes; }

    TimerId runAfter(Duration delay, TimerCallback cb) {
        return server_.runAfter(delay, std::move(cb));
    }

    void start() { server_.start(); }
    void stop(Duration gracePeriod = std::chrono::seconds(10),
              std::function<void()> completed = {}) {
        server_.stop(gracePeriod, std::move(completed));
    }

    static void sendText(const TcpConnectionPtr& conn, const std::string& message);
    static void sendBinary(const TcpConnectionPtr& conn, const std::string& message);
    static void close(const TcpConnectionPtr& conn, uint16_t code = 1000,
                      const std::string& reason = {});

private:
    struct ConnectionContext {
        HttpContext http;
        bool upgraded{false};
        bool closeSent{false};
        Opcode fragmentedOpcode{Opcode::Continuation};
        std::string fragmentedPayload;
    };

    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf);
    bool handleUpgrade(const TcpConnectionPtr& conn, Buffer* buf,
                       ConnectionContext* context);
    bool parseFrames(const TcpConnectionPtr& conn, Buffer* buf,
                     ConnectionContext* context);
    void protocolError(const TcpConnectionPtr& conn, ConnectionContext* context,
                       uint16_t code, const std::string& reason);

    static std::string makeFrame(Opcode opcode, const std::string& payload,
                                 bool fin = true);

    TcpServer server_;
    OpenCallback openCallback_;
    MessageCallback messageCallback_;
    CloseCallback closeCallback_;
    HttpCallback httpCallback_;
    size_t maxMessageSize_{16 * 1024 * 1024};
};

} // namespace muduo
