#include "net/HttpServer.h"
#include "net/Buffer.h"
#include "net/HttpContext.h"
#include "net/InetAddress.h"
#include "net/TcpConnection.h"


namespace muduo{

    HttpServer::HttpServer(EventLoop* loop, const InetAddress& listenAddr)
    : server_(loop, listenAddr, "HttpServer") {
    server_.setConnectionCallback(
        [this](const TcpConnectionPtr& conn) { onConnection(conn); });
    server_.setMessageCallback(
        [this](const TcpConnectionPtr& conn, Buffer* buf) {
            onMessage(conn, buf);
        });
}

    void HttpServer::onConnection(const TcpConnectionPtr& conn){
        if(conn->connected()){
            conn->setContext_(HttpContext());
        }
    }

    void HttpServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf){
        HttpContext* context = std::any_cast<HttpContext>(conn->getMutableContext());

        // 解析，返回 false 说明报文格式有问题
        if (!context->parseRequest(buf)) {
            // 回一个 400 Bad Request 然后关连接
            conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
            conn->shutdown();
            return;
        }
        
        // 解析完整了才处理，没完整就等下次数据来
        if (context->gotAll()) {
            onRequest(conn, context->request());
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
        }
    }
}