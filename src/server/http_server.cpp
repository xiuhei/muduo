#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/TcpServer.h"
#include "net/HttpServer.h"


int main(int argc, char* argv[]) {
    muduo::EventLoop loop;
    muduo::InetAddress addr(8080);
    muduo::HttpServer server(&loop, addr);


    int threadNum=0;
    if(argc>1){threadNum=std::stoi(argv[1]);}
    server.setThreadNum(threadNum);

    server.setHttpCallback([](const muduo::HttpRequest& req, muduo::HttpResponse* resp) {
    const std::string& path = req.path();
    
    if (path == "/") {
        resp->setStatusCode(muduo::HttpResponse::StatusCode::Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/html");
        resp->setBody("<h1>Hello</h1>");
    } else if (path == "/hello") {
        resp->setStatusCode(muduo::HttpResponse::StatusCode::Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/plain");
        resp->setBody("hello world");
    } else {
        resp->setStatusCode(muduo::HttpResponse::StatusCode::NotFound);
        resp->setStatusMessage("Not Found");
        resp->setBody("404 Not Found");
    }
});

    server.start();
    loop.loop();
}