# Muduo

仿 [muduo](https://github.com/chenshuo/muduo) 实现的 C++ 网络库，基于 Reactor 模式，提供 TCP 服务器核心组件，并在此之上实现了一个简单的 HTTP 服务器。

## 技术栈
- 多 Reactor 多线程模型（主 Reactor 负责 accept，从 Reactor 池负责 I/O）
- epoll（LT 模式）+ 非阻塞 I/O
- 自实现 Buffer 缓冲区，支持自动扩容与 `readFd`/`retrieve` 等操作
- HTTP/1.1 请求解析（状态机）与响应构造
- C++17，CMake 构建
- 基于 spdlog 的异步日志（文件记录 INFO+、控制台输出 WARN+、3 秒周期刷新、满队列背压、退出前排空）

## 定时器与超时策略

`EventLoop` 通过 `timerfd` 将时间事件接入 epoll，支持一次性任务、周期性任务及取消操作。`TcpServer` 和 `HttpServer` 对外提供相同的定时器接口：

- `runAt`：在指定时间点执行一次
- `runAfter`：延迟指定时长后执行一次
- `runEvery`：按指定间隔重复执行
- `cancelTimer`：取消尚未执行或仍在重复执行的定时器

TCP 和 HTTP 层基于该机制提供以下超时策略：

| 模块 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| 连接空闲超时 | `TcpServer::setConnectionIdleTimeout` | 0（不启用） | 连接在指定时间内无读写活动则自动关闭 |
| 写入超时 | `TcpServer::setConnectionWriteTimeout` | 0（不启用） | 数据在输出缓冲区积压超过指定时间则关闭连接 |
| 优雅关闭超时 | `TcpConnection::setShutdownTimeout` | 5 秒 | 调用 shutdown 后对端超时未发 FIN 则强制关闭 |
| HTTP 请求解析超时 | `HttpServer::setRequestTimeout` | 15 秒 | 防御 Slowloris 攻击，客户端缓慢发送不完整请求 |
| HTTP Keep-Alive 空闲超时 | `HttpServer::setKeepAliveTimeout` | 60 秒 | Keep-Alive 连接响应发送后空闲超时则关闭 |

> **注意**：表中的服务器超时配置应在调用 `start()` 前完成；普通定时任务可以在服务运行期间通过上述接口创建或取消。

## 目录结构
```
src/
├── base/                         # 通用基础设施：日志、不可复制基类
├── net/
│   ├── reactor/                  # Reactor 核心、epoll、I/O 线程及信号事件
│   ├── timer/                    # 定时器模型与基于 timerfd 的时间事件队列
│   └── tcp/                      # socket 封装、地址、缓冲区及 TCP 连接/服务
├── http/                         # 基于 TCP 的 HTTP/1.1 协议与服务器
└── examples/                     # 可执行示例，不属于库实现
    ├── echo_server.cpp
    ├── http_server.cpp
    └── timeout_example.cpp
```

总体依赖方向为：`base <- net(reactor + timer) <- net/tcp <- http <- examples`。
`reactor` 与 `timer` 共同组成事件调度核心（`EventLoop` 对外暴露定时器接口，
`TimerQueue` 接入 Reactor），TCP 层使用该核心，HTTP 层只通过 TCP 层提供服务；
底层模块不得反向包含 HTTP 或示例代码。新增代码应按职责放入对应目录，避免再次
形成扁平的 `net/` 聚合目录。

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
该示例通过 `runEvery` 周期性记录 tick；访问 `http://localhost:8080/api/timeout` 可通过 `runAfter` 创建延迟任务，具体行为见示例源码。

## 压测
```bash
wrk -t4 -c100 -d10s http://127.0.0.1:8080/
```
本机测得http_server示例函数 22 万 QPS（单线程模式）      同设备环境对比原muduo 30 万 qps。
本机测得http_server示例函数 86 万 QPS（4线程并发模式）    同设备环境对比原muduo 69 万 qps。
