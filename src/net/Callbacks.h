#pragma once

#include<functional>
#include<memory>

namespace muduo
{

class TcpConnection;
class Buffer;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*)>;
using ConnectionCallback=std::function<void(const TcpConnectionPtr&)>;
using CloseCallback=std::function<void(const TcpConnectionPtr&)>;

}// namespace muduo