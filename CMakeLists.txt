cmake_minimum_required(VERSION 3.16)

project(muduo LANGUAGES CXX)

include(FetchContent)

# 优先使用系统安装的 spdlog；未安装时由 CMake 自动获取固定版本。
find_package(spdlog 1.12 QUIET)
if(NOT spdlog_FOUND)
    FetchContent_Declare(
        spdlog
        URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(spdlog)
endif()

# 未指定构建类型时默认使用 Debug
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 生成 compile_commands.json，供 clangd/IDE 索引使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 基础设施：不依赖网络层
set(MUDUO_BASE_SOURCES
    src/base/Logger.cpp
)

# Reactor：事件分发、I/O 线程与进程信号接入
set(MUDUO_REACTOR_SOURCES
    src/net/reactor/Channel.cpp
    src/net/reactor/Poller.cpp
    src/net/reactor/EventLoop.cpp
    src/net/reactor/EventLoopThread.cpp
    src/net/reactor/EventLoopThreadPool.cpp
    src/net/reactor/SignalWatcher.cpp
)

# Timer：依附 Reactor 的时间事件调度
set(MUDUO_TIMER_SOURCES
    src/net/timer/TimerQueue.cpp
)

# TCP：socket 封装、连接生命周期与字节流传输
set(MUDUO_TCP_SOURCES
    src/net/tcp/Acceptor.cpp
    src/net/tcp/Buffer.cpp
    src/net/tcp/InetAddress.cpp
    src/net/tcp/Socket.cpp
    src/net/tcp/TcpConnection.cpp
    src/net/tcp/TcpServer.cpp
)

# HTTP：建立在 TCP 之上的应用协议层
set(MUDUO_HTTP_SOURCES
    src/http/HttpContext.cpp
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp
    src/http/HttpServer.cpp
)

add_library(muduo
    ${MUDUO_BASE_SOURCES}
    ${MUDUO_REACTOR_SOURCES}
    ${MUDUO_TIMER_SOURCES}
    ${MUDUO_TCP_SOURCES}
    ${MUDUO_HTTP_SOURCES}
)

target_include_directories(muduo PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_options(muduo PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(muduo PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
target_link_libraries(muduo PUBLIC pthread spdlog::spdlog)

# 示例程序：回显服务器
add_executable(echo_server src/examples/echo_server.cpp)
target_link_libraries(echo_server PRIVATE muduo)

# 示例程序：HTTP 服务器
add_executable(http_server src/examples/http_server.cpp)
target_link_libraries(http_server PRIVATE muduo)

# 示例程序：定时器接口示例与测试
add_executable(timeout_example src/examples/timeout_example.cpp)
target_link_libraries(timeout_example PRIVATE muduo)
