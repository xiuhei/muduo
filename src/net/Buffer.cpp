#include "net/Buffer.h"

#include <sys/uio.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <algorithm>  

namespace muduo {

namespace{
    static const char CRLF[] ="\r\n";
}

Buffer::Buffer(size_t initialSize)
    : buffer_(kCheapPrepend + initialSize),
      readIndex_(kCheapPrepend),
      writeIndex_(kCheapPrepend) {}

size_t Buffer::readableBytes() const {
    return writeIndex_ - readIndex_;
}

size_t Buffer::writableBytes() const {
    return buffer_.size() - writeIndex_;
}

size_t Buffer::prependableBytes() const {
    return readIndex_;
}


const char* Buffer::findCRLF() const {
    const char* start = peek();
    const char* end = beginWrite();  // 可读数据的末尾
    const char* pos = std::search(start, end, CRLF,CRLF + 2);
    return pos == end ? nullptr : pos;
}


const char* Buffer::peek() const {
    return begin() + readIndex_;
}

void Buffer::retrieve(size_t len) {
    if (len < readableBytes()) {
        // 只消费一部分数据，保留剩余可读区间给后续业务处理。
        readIndex_ += len;
    } else {
        retrieveAll();
    }
}

void Buffer::retrieveAll() {
    // 数据全部消费后回到初始布局，保留 prepend 区域并复用已有容量。
    readIndex_ = kCheapPrepend;
    writeIndex_ = kCheapPrepend;
}

std::string Buffer::retrieveAsString(size_t len) {
    len = std::min(len, readableBytes());
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

std::string Buffer::retrieveAllAsString() {
    return retrieveAsString(readableBytes());
}

void Buffer::append(const char* data, size_t len) {
    ensureWritableBytes(len);
    std::copy(data, data + len, beginWrite());
    hasWritten(len);
}

void Buffer::append(const std::string& data) {
    append(data.data(), data.size());
}

ssize_t Buffer::readFd(int fd, int* savedErrno) {
    char extrabuf[65536];
    iovec vec[2];
    const size_t writable = writableBytes();

    // readv 先写入内部可写空间，不够时再写入栈上临时缓冲，减少一次 ioctl/FIONREAD。
    vec[0].iov_base = begin() + writeIndex_;
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = writable < sizeof(extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        *savedErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        writeIndex_ += static_cast<size_t>(n);
    } else {
        writeIndex_ = buffer_.size();
        append(extrabuf, static_cast<size_t>(n) - writable);
    }
    return n;
}

char* Buffer::begin() {
    return buffer_.data();
}

const char* Buffer::begin() const {
    return buffer_.data();
}

void Buffer::ensureWritableBytes(size_t len) {
    if (writableBytes() < len) {
        // 可写尾部不足时，要么整理已消费空间，要么扩容。
        makeSpace(len);
    }
}

void Buffer::makeSpace(size_t len) {
    if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
        buffer_.resize(writeIndex_ + len);
        return;
    }

    // 前面已经被 retrieve 的空间可以复用，把可读数据搬回 kCheapPrepend 后面。
    const size_t readable = readableBytes();
    std::copy(begin() + readIndex_, begin() + writeIndex_, begin() + kCheapPrepend);
    readIndex_ = kCheapPrepend;
    writeIndex_ = readIndex_ + readable;
}

char* Buffer::beginWrite() {
    // writeIndex_ 指向下一次 append/readv 写入的位置。
    return begin() + writeIndex_;
}

const char* Buffer::beginWrite() const {
    return begin() + writeIndex_;
}

void Buffer::hasWritten(size_t len) {
    // 外部或 readv 写入 len 字节后推进写索引，使其变成可读数据。
    writeIndex_ += len;
}

} // namespace muduo
