// timeout_example.cpp —— 定时器接口简单演示
//
// 启动 HTTP 服务器，演示 runAfter / runEvery 的基本用法。
// 浏览器打开 http://localhost:8080 即可测试。
//
// 用法：./timeout_example [端口号，默认 8080]

#include "net/reactor/EventLoop.h"
#include "http/HttpServer.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "net/tcp/InetAddress.h"
#include "net/reactor/SignalWatcher.h"
#include "base/Logger.h"

#include <iostream>
#include <atomic>
#include <csignal>

using namespace muduo;

int main(int argc, char* argv[]) {
    SignalWatcher::block({SIGINT, SIGTERM});
    LogGuard logGuard("muduo");
    int port = (argc > 1) ? std::stoi(argv[1]) : 8080;

    EventLoop loop;
    SignalWatcher signals(&loop, {SIGINT, SIGTERM});
    HttpServer server(&loop, InetAddress(port));

    std::atomic<int> tickCount{0};

    // ---- 演示 runEvery：每 5 秒在日志文件中打印一次 ----
    server.runEvery(std::chrono::seconds(5), [&] {
        LOG_INFO("[runEvery] tick #{}", ++tickCount);
    });

    // ---- 演示 runAfter：启动 1 秒后打印就绪信息 ----
    server.runAfter(std::chrono::seconds(1), [port] {
        LOG_INFO("服务器已就绪，浏览器打开 http://localhost:{}", port);
    });

    // ---- HTTP 路由 ----
    server.setHttpCallback([&](const HttpRequest& req, HttpResponse* resp) {
        const std::string& path = req.path();

        // 首页：简单 HTML 页面
        if (path == "/") {
            std::string html = R"(<!DOCTYPE html><html><head><meta charset='utf-8'>
<title>定时器演示</title></head><body>
<h2>Muduo 定时器接口演示</h2>
<ul>
  <li><a href='/api/timeout'>GET /api/timeout</a> — 安排一个 3 秒后触发的 runAfter 任务（查看日志）</li>
  <li>控制台每 5 秒打印一次 runEvery tick（当前 tick: )" + std::to_string(tickCount) + R"()</li>
</ul>
</body></html>)";

            resp->setStatusCode(HttpResponse::StatusCode::Ok);
            resp->setStatusMessage("OK");
            resp->setContentType("text/html; charset=utf-8");
            resp->setBody(html);
            return;
        }

        // /api/timeout：触发一个 runAfter 延迟任务
        if (path == "/api/timeout") {
            server.runAfter(std::chrono::seconds(3), [] {
                LOG_INFO("[runAfter] 3 秒延迟任务触发！");
            });

            LOG_INFO("[runAfter] 已安排一个 3 秒后触发的延迟任务");

            resp->setStatusCode(HttpResponse::StatusCode::Ok);
            resp->setStatusMessage("OK");
            resp->setContentType("text/plain; charset=utf-8");
            resp->setBody("已安排 3 秒后触发的延迟任务，请查看日志\n");
            return;
        }

        // 404
        resp->setStatusCode(HttpResponse::StatusCode::NotFound);
        resp->setStatusMessage("Not Found");
        resp->setContentType("text/plain");
        resp->setBody("404 Not Found\n");
    });

    server.setThreadNum(1);
    server.start();
    signals.setCallback([&](int) {
        server.stop(std::chrono::seconds(10), [&loop] { loop.quit(); });
    });
    loop.loop();

    return 0;
}
