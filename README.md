# Muduo

仿 [muduo](https://github.com/chenshuo/muduo) 实现的 C++ 网络库，基于 Reactor 模式，提供 TCP 服务器核心组件，并在此之上实现了一个简单的 HTTP 服务器。

## 技术栈
- 多 Reactor 多线程模型（主 Reactor 负责 accept，从 Reactor 池负责 I/O）
- epoll（LT 模式）+ 非阻塞 I/O
- 自实现 Buffer 缓冲区，支持自动扩容与 `readFd`/`retrieve` 等操作
- HTTP/1.1 请求解析（状态机）与响应构造
- C++17，CMake 构建

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
│   └── HttpContext / HttpRequest /
│       HttpResponse / HttpServer         HTTP 协议层
└── server/
    ├── echo_server.cpp    # 多线程回显服务器示例
    └── http_server.cpp    # HTTP 服务器示例
```

## 编译
```bash
mkdir -p build && cd build
cmake ..
make -j
```
产物为静态库 `libmuduo.a`，以及两个示例可执行文件 `echo_server`、`http_server`。

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

## 压测
```bash
wrk -t4 -c100 -d10s http://127.0.0.1:8080/
```
本机测得约 22 万 QPS（环境差异下结果会有波动，仅供参考）。
