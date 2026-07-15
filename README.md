# Muduo

仿 [muduo](https://github.com/chenshuo/muduo) 实现的 C++ 网络库，基于 Reactor 模式，提供 TCP 服务器核心组件，并在此之上实现了一个简单的 HTTP 服务器。

## 技术栈
- 多 Reactor 多线程模型（主 Reactor 负责 accept，从 Reactor 池负责 I/O）
- epoll（LT 模式）+ 非阻塞 I/O
- 自实现 Buffer 缓冲区，支持自动扩容与 `readFd`/`retrieve` 等操作
- HTTP/1.1 请求解析（状态机）与响应构造
- C++17，CMake 构建

## 超时机制

项目实现了完整的超时管理，基于 `timerfd` + epoll 集成，共包含以下模块：

| 模块 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| 连接空闲超时 | `TcpServer::setConnectionIdleTimeout` | 0（不启用） | 连接在指定时间内无读写活动则自动关闭 |
| 写入超时 | `TcpServer::setConnectionWriteTimeout` | 0（不启用） | 数据在输出缓冲区积压超过指定时间则关闭连接 |
| 优雅关闭超时 | `TcpConnection::setShutdownTimeout` | 5 秒 | 调用 shutdown 后对端超时未发 FIN 则强制关闭 |
| HTTP 请求解析超时 | `HttpServer::setRequestTimeout` | 15 秒 | 防御 Slowloris 攻击，客户端缓慢发送不完整请求 |
| HTTP Keep-Alive 空闲超时 | `HttpServer::setKeepAliveTimeout` | 60 秒 | Keep-Alive 连接响应发送后空闲超时则关闭 |

核心实现：
- **Timer/TimerId**：定时器抽象，基于 `std::chrono::steady_clock`，使用序列号保证取消操作安全
- **TimerQueue**：通过 `timerfd` 与 epoll 集成，所有定时器按到期时间排序；`addTimer`/`cancel` 通过 `runInLoop` 保证线程安全
- **EventLoop 集成**：`runAt`/`runAfter`/`runEvery`/`cancel` 接口，poll 超时联动最近定时器的到期时间
- **TcpServer 周期扫描**：每 3 秒扫描所有连接，关闭超时连接，复用 TcpConnection 的 `shutdown()` 优雅关闭流程

> **注意**：超时配置应在调用 `start()` 之前完成设置。

## 目录结构
```
src/
├── base/                  # 基础设施（NonCopyable 等）
├── net/                   # 核心网络库
│   ├── EventLoop / Channel / Poller      事件驱动核心（epoll 封装）
│   ├── EventLoopThread(Pool)             I/O 线程与线程池
│   ├── Acceptor / Socket / InetAddress   连接接入
│   ├── TcpServer / TcpConnection         连接管理与读写
│   ├── Buffer                            读写缓冲区
│   ├── Timer / TimerId / TimerQueue      定时器与超时管理
│   └── HttpContext / HttpRequest /
│       HttpResponse / HttpServer         HTTP 协议层
└── server/
    ├── echo_server.cpp      # 多线程回显服务器示例
    ├── http_server.cpp     # HTTP 服务器示例
    └── timeout_example.cpp # 定时器接口演示
```

## 编译
```bash
mkdir -p build && cd build
cmake ..
make -j
```
产物为静态库 `libmuduo.a`，以及三个示例可执行文件 `echo_server`、`http_server`、`timeout_example`。

## 运行示例

### Echo Server
```bash
./build/echo_server [线程数，默认 4]
```
默认监听 `0.0.0.0:8888`，将收到的数据原样返回。

### HTTP Server
```bash
./build/http_server
```
监听 `0.0.0.0:8080`，内置路由：
- `GET /`      返回 `<h1>Hello</h1>`
- `GET /hello` 返回 `hello world`
- 其他路径返回 `404 Not Found`

可通过 `HttpServer::setHttpCallback` 自定义请求处理逻辑。

### Timeout Example
```bash
./build/timeout_example [端口号，默认 8080]
```
演示 `runAfter` / `runEvery` 定时器接口的基本用法：
- 启动后每 5 秒在控制台打印一次 tick（`runEvery`）
- 浏览器访问 `http://localhost:8080/api/timeout` 触发一个 3 秒延迟任务（`runAfter`）

## 压测
```bash
wrk -t4 -c100 -d10s http://127.0.0.1:8080/
```
本机测得约 22 万 QPS（单线程模式）      同设备环境对比原muduo 30 万 qps。
本机测得约 86 万 QPS（4线程并发模式）   同设备环境对比原muduo 69 万 qps。

