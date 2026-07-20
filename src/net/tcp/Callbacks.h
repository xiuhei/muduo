#pragma once

#include<functional>
#include<memory>

namespace muduo
{

class TcpConnection;
class Buffer;
// TCP 层用 shared_ptr 保证用户回调执行期间连接对象仍然存活。
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*)>;
using ConnectionCallback=std::function<void(const TcpConnectionPtr&)>;
using CloseCallback=std::function<void(const TcpConnectionPtr&)>;

}// namespace muduo
