#pragma once

#include "net/TcpServer.h"
#include "net/HttpRequest.h"
#include "net/HttpResponse.h"

#include <functional>
#include <chrono>

namespace muduo {

class HttpServer {
public:
    using HttpCallback = std::function<void(const HttpRequest&, HttpResponse*)>;

    HttpServer(EventLoop* loop, const InetAddress& listenAddr);

    void setThreadNum(int num){server_.setThreadNum(num);}

    void setHttpCallback(HttpCallback cb) { httpCallback_ = std::move(cb); }
    void start() { server_.start(); }

    // 设置请求解析超时，默认 15 秒。防止 Slowloris 攻击——客户端缓慢发送不完整请求
    void setRequestTimeout(std::chrono::seconds timeout) { requestTimeout_ = timeout; }
    // 设置 HTTP Keep-Alive 空闲超时，默认 60 秒。复用连接在发送完响应后若空闲超时则关闭
    void setKeepAliveTimeout(std::chrono::seconds timeout) { keepAliveTimeout_ = timeout; }


private:
    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf);
    void onRequest(const TcpConnectionPtr& conn, const HttpRequest& req);

    TcpServer server_;
    HttpCallback httpCallback_;

    std::chrono::seconds requestTimeout_{15};     // 默认 15 秒请求解析超时
    std::chrono::seconds keepAliveTimeout_{60};   // 默认 60 秒 Keep-Alive 空闲超时

};

} // namespace muduo