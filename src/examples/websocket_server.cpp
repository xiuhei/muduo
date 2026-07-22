#include "base/Logger.h"
#include "net/reactor/EventLoop.h"
#include "net/reactor/SignalWatcher.h"
#include "net/tcp/InetAddress.h"
#include "net/tcp/TcpConnection.h"
#include "websocket/WebSocketServer.h"

#include <csignal>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    // 使用方法：
    // 1. 编译并启动服务：
    //      cmake --build build --target websocket_server
    //      ./build/websocket_server
    //    默认监听 8081；也可以用 ./build/websocket_server 9000 4
    //    指定端口为 9000、IO 线程数为 4。
    //
    // 2. 浏览器访问 http://127.0.0.1:8081/，在页面中直接发送消息即可。
    //    页面会自动建立 WebSocket 连接
    const uint16_t port = argc > 1
        ? static_cast<uint16_t>(std::stoul(argv[1]))
        : 8081;
    const int threadNum = argc > 2 ? std::stoi(argv[2]) : 0;

    muduo::SignalWatcher::block({SIGINT, SIGTERM});
    muduo::LogGuard logGuard("muduo");
    muduo::EventLoop loop;
    muduo::SignalWatcher signals(&loop, {SIGINT, SIGTERM});
    muduo::WebSocketServer server(&loop, muduo::InetAddress(port));

    server.setThreadNum(threadNum);
    server.setHttpCallback([](const muduo::HttpRequest& request,
                              muduo::HttpResponse* response) {
        if (request.method() != muduo::HttpRequest::GET || request.path() != "/") {
            response->setStatusCode(muduo::HttpResponse::StatusCode::NotFound);
            response->setStatusMessage("Not Found");
            response->setContentType("text/plain; charset=utf-8");
            response->setBody("404 Not Found\n");
            return;
        }
        response->setContentType("text/html; charset=utf-8");
        response->setBody(R"HTML(<!doctype html>
<meta charset="utf-8">
<title>WebSocket Echo</title>
<style>body{font:16px sans-serif;max-width:720px;margin:40px auto;padding:0 16px}input{width:70%;padding:8px}button{padding:8px 14px}#log{white-space:pre-wrap;background:#f4f4f4;padding:12px;min-height:160px}</style>
<h1>WebSocket Echo</h1>
<p id="status">正在连接...</p>
<input id="message" value="hello"><button id="send" disabled>发送</button>
<pre id="log"></pre>
<script>
const status = document.querySelector('#status');
const input = document.querySelector('#message');
const send = document.querySelector('#send');
const log = document.querySelector('#log');
const ws = new WebSocket(`ws://${location.host}/`);
const write = text => log.textContent += text + '\n';
ws.onopen = () => { status.textContent = '已连接'; send.disabled = false; };
ws.onmessage = event => write('收到: ' + event.data);
ws.onerror = () => write('连接发生错误');
ws.onclose = event => { status.textContent = '连接已关闭'; send.disabled = true; write(`关闭: ${event.code}`); };
send.onclick = () => { ws.send(input.value); write('发送: ' + input.value); };
input.onkeydown = event => { if (event.key === 'Enter' && !send.disabled) send.click(); };
</script>)HTML");
    });
    server.setOpenCallback([](const muduo::TcpConnectionPtr& conn,
                              const muduo::HttpRequest& request) {
        // 握手完成后发送第一条文本消息，客户端的 onmessage 会收到它。
        LOG_INFO("WebSocket opened: peer={} path={}",
                 conn->peerAddress().toIpPort(), request.path());
        muduo::WebSocketServer::sendText(conn, "WebSocket connection established");
    });
    server.setMessageCallback([](const muduo::TcpConnectionPtr& conn,
                                 const std::string& message,
                                 muduo::WebSocketServer::Opcode opcode) {
        // 收到的数据原样返回，便于确认文本帧和二进制帧的双向通信正常。
        if (opcode == muduo::WebSocketServer::Opcode::Text) {
            LOG_INFO("Received text message: peer={} bytes={}",
                     conn->peerAddress().toIpPort(), message.size());
            muduo::WebSocketServer::sendText(conn, message);
        } else if (opcode == muduo::WebSocketServer::Opcode::Binary) {
            LOG_INFO("Received binary message: peer={} bytes={}",
                     conn->peerAddress().toIpPort(), message.size());
            muduo::WebSocketServer::sendBinary(conn, message);
        }
    });
    server.setCloseCallback([](const muduo::TcpConnectionPtr& conn) {
        LOG_INFO("WebSocket closed: peer={}", conn->peerAddress().toIpPort());
    });

    signals.setCallback([&](int) {
        server.stop(std::chrono::seconds(10), [&loop] { loop.quit(); });
    });
    server.start();
    // 日志系统写入 logs 目录
    std::cout << "WebSocket echo server is listening at ws://127.0.0.1:"
              << port << "/\n"
              << "Open http://127.0.0.1:" << port << "/ in a browser to test it.\n"
              << "Press Ctrl+C to stop. Logs are written under ./logs/.\n"
              << std::flush;
    LOG_INFO("websocket_server listening on 0.0.0.0:{} with {} IO threads",
             port, threadNum);
    loop.loop();
}
