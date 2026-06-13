#pragma once

#include <vector>
#include <string>


namespace muduo
{   

   
class Buffer {
public:
    // 预留给上层协议在数据前追加长度/头部字段等信息的空间
    static constexpr size_t kCheapPrepend = 8;
    static constexpr size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize);
    ~Buffer() = default;

    size_t readableBytes() const;
    size_t writableBytes() const;
    size_t prependableBytes() const;

    // 返回可读数据起始地址，调用 retrieve/append 后该指针可能失效。
    const char* peek() const;
    // 消费 len 字节；如果 len 覆盖全部可读数据，则重置读写索引。
    void retrieve(size_t len);
    void retrieveAll();
    std::string retrieveAsString(size_t len);
    std::string retrieveAllAsString();

    // 追加数据时会优先复用已消费空间，不够再扩容 vector。
    void append(const char* data, size_t len);
    void append(const std::string& data);

    // 从非阻塞 fd 读取数据，返回 readv 结果；出错时通过 savedErrno 传回 errno。
    ssize_t readFd(int fd, int* savedErrno);

private:
    std::vector<char> buffer_{};
    size_t readIndex_{0};
    size_t writeIndex_{0};

    char* begin();
    const char* begin() const;
    void ensureWritableBytes(size_t len);
    void makeSpace(size_t len);
    char* beginWrite();
    const char* beginWrite() const;
    void hasWritten(size_t len);


};


}// namespace muduo