cmake_minimum_required(VERSION 3.16)

project(muduo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(muduo
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
)

target_include_directories(muduo PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_options(muduo PRIVATE -Wall -Wextra -Wpedantic)

add_executable(echo_server src/server/echo_server.cpp)
target_link_libraries(echo_server PRIVATE muduo pthread)