cmake_minimum_required(VERSION 3.16)

project(muduo LANGUAGES CXX)

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
    src/net/Acceptor.cpp
    src/net/Buffer.cpp
    src/net/Channel.cpp
    src/net/Poller.cpp
    src/net/EventLoop.cpp
    src/net/EventLoopThread.cpp
    src/net/EventLoopThreadPool.cpp
    src/net/InetAddress.cpp
    src/net/Socket.cpp
    src/net/TcpConnection.cpp
    src/net/TcpServer.cpp
    src/net/HttpContext.cpp
    src/net/HttpRequest.cpp
    src/net/HttpResponse.cpp
    src/net/HttpServer.cpp
)

add_library(muduo ${MUDUO_SOURCES})

target_include_directories(muduo PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_options(muduo PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(muduo PUBLIC pthread)

# 示例程序：回显服务器
add_executable(echo_server src/server/echo_server.cpp)
target_link_libraries(echo_server PRIVATE muduo)

# 示例程序：HTTP 服务器
add_executable(http_server src/server/http_server.cpp)
target_link_libraries(http_server PRIVATE muduo)
