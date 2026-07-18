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

# muduo 核心库源文件
set(MUDUO_SOURCES
    src/base/Logger.cpp
    src/net/Acceptor.cpp
    src/net/Buffer.cpp
    src/net/Channel.cpp
    src/net/Poller.cpp
    src/net/EventLoop.cpp
    src/net/EventLoopThread.cpp
    src/net/EventLoopThreadPool.cpp
    src/net/InetAddress.cpp
    src/net/Socket.cpp
    src/net/SignalWatcher.cpp
    src/net/TcpConnection.cpp
    src/net/TcpServer.cpp
    src/net/TimerQueue.cpp
    src/net/HttpContext.cpp
    src/net/HttpRequest.cpp
    src/net/HttpResponse.cpp
    src/net/HttpServer.cpp
)

add_library(muduo ${MUDUO_SOURCES})

target_include_directories(muduo PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_options(muduo PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(muduo PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
target_link_libraries(muduo PUBLIC pthread spdlog::spdlog)

# 示例程序：回显服务器
add_executable(echo_server src/server/echo_server.cpp)
target_link_libraries(echo_server PRIVATE muduo)

# 示例程序：HTTP 服务器
add_executable(http_server src/server/http_server.cpp)
target_link_libraries(http_server PRIVATE muduo)

# 示例程序：定时器接口示例与测试
add_executable(timeout_example src/server/timeout_example.cpp)
target_link_libraries(timeout_example PRIVATE muduo)
