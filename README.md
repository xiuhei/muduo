# Muduo-HTTP

基于 C++ 实现的高性能 HTTP 服务器，参考 muduo 网络库设计。

## 技术栈
- 多 Reactor 多线程模型（主从 Reactor）
- 非阻塞 IO + epoll ET/LT
- 自实现 Buffer 缓冲区
- HTTP/1.1 协议解析（状态机）

## 模块
- EventLoop / Channel / Poller：事件驱动核心
- TcpServer / TcpConnection：连接管理
- HttpContext / HttpRequest / HttpResponse：HTTP 层

## 编译运行
mkdir build && cd build
cmake .. && make
./http_server

## 压测
wrk -t4 -c100 -d10s http://127.0.0.1:8080/
Requests/sec: 222598（22万 QPS）