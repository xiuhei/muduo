#pragma once

#include "net/TcpServer.h"
#include "net/HttpRequest.h"
#include "net/HttpResponse.h"

#include <functional>

namespace muduo {

class HttpServer {
public:
    using HttpCallback = std::function<void(const HttpRequest&, HttpResponse*)>;

    HttpServer(EventLoop* loop, const InetAddress& listenAddr);

    void setHttpCallback(HttpCallback cb) { httpCallback_ = std::move(cb); }
    void start() { server_.start(); }

private:
    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf);
    void onRequest(const TcpConnectionPtr& conn, const HttpRequest& req);

    TcpServer server_;
    HttpCallback httpCallback_;
};

} // namespace muduo