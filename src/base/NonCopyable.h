#pragma once

namespace muduo
{

// 供资源拥有者私有继承，统一禁止复制，避免 fd、线程等资源被重复释放。
class NonCopyable
{
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;
public:
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

};

} 
