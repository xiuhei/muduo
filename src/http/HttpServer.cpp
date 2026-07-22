#include "http/HttpServer.h"
#include "base/Logger.h"
#include "net/tcp/Buffer.h"
#include "http/HttpContext.h"
#include "net/tcp/InetAddress.h"
#include "net/tcp/TcpConnection.h"

#include <chrono>

namespace muduo{

    HttpServer::HttpServer(EventLoop* loop, const InetAddress& listenAddr)
    : server_(loop, listenAddr, "HttpServer") {
        server_.setConnectionCallback(
            [this](const TcpConnectionPtr& conn) { onConnection(conn); });
        server_.setMessageCallback(
            [this](const TcpConnectionPtr& conn, Buffer* buf) {
                onMessage(conn, buf);
            });
        // 将 HTTP 的超时配置传递给底层 TcpServer
        server_.setConnectionIdleTimeout(keepAliveTimeout_);
    }

    void HttpServer::onConnection(const TcpConnectionPtr& conn){
        if(conn->connected()){
            // 跨越 HTTP/TCP 边界：TCP context 保存此连接独立的增量解析器。
            conn->setContext_(HttpContext());
        }
    }

    void HttpServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf){
        // context 在连接建立回调中初始化，消息回调与其在同一 IO loop 串行执行。
        HttpContext* context = std::any_cast<HttpContext>(conn->getMutableContext());

        // Slowloris 防御：检查请求解析是否超时
        if (requestTimeout_.count() > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - context->requestStartTime);
            if (elapsed >= requestTimeout_) {
                LOG_WARN("HTTP request timeout from {} after {} seconds",
                         conn->peerAddress().toIpPort(), elapsed.count());
                // 请求解析超时，返回 408 Request Timeout 并关闭连接
                conn->send("HTTP/1.1 408 Request Timeout\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                conn->shutdown();
                return;
            }
        }

        // 解析，返回 false 说明报文格式有问题
        if (!context->parseRequest(buf)) {
            LOG_WARN("invalid HTTP request from {}; returning 400",
                     conn->peerAddress().toIpPort());
            // 回一个 400 Bad Request 然后关连接
            conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
            conn->shutdown();
            return;
        }

        // 解析完整了才处理，没完整就等下次数据来
        if (context->gotAll()) {
            const HttpRequest& request = context->request();
            LOG_DEBUG("HTTP request: peer={} method={} path={}",
                      conn->peerAddress().toIpPort(),
                      static_cast<int>(request.method()), request.path());
            onRequest(conn, request);
            context->reset(); // 处理完重置，准备下一个请求（keep-alive）
        }
    }


    void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& req){
        // 判断是否需要关闭连接
        const std::string& connection = req.getHeader("Connection");
        bool close = (connection == "close") ||
                 (req.version() == HttpRequest::Version::Http10);

        HttpResponse response;
        response.setCloseConnection(close);

        // 调用用户注册的回调，让用户填充 response
        if (httpCallback_) {
            httpCallback_(req, &response);
        }
         // 序列化发出去
        Buffer buf;
        response.appendToBuffer(&buf);
        conn->send(buf.retrieveAllAsString());

        // 如果需要关闭连接
        if (response.closeConnection()) {
            conn->shutdown();
        } else {
            // Keep-alive 连接：确保空闲超时已设置，响应发送后若客户端长时间不发新请求则主动关闭
            if (keepAliveTimeout_.count() > 0) {
                conn->setIdleTimeout(keepAliveTimeout_);
            }
        }
    }
}
