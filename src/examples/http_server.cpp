#include "net/reactor/EventLoop.h"
#include "net/tcp/InetAddress.h"
#include "net/tcp/TcpServer.h"
#include "http/HttpServer.h"
#include "base/Logger.h"
#include "net/reactor/SignalWatcher.h"

#include <csignal>


int main(int argc, char* argv[]) {
    muduo::SignalWatcher::block({SIGINT, SIGTERM});
    muduo::LogGuard logGuard("muduo");
    muduo::EventLoop loop;
    muduo::SignalWatcher signals(&loop, {SIGINT, SIGTERM});
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
    signals.setCallback([&](int) {
        server.stop(std::chrono::seconds(10), [&loop] { loop.quit(); });
    });
    LOG_INFO("http_server listening on 0.0.0.0:8080 with {} IO threads", threadNum);
    loop.loop();
}
