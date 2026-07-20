#pragma once
#include "http/HttpRequest.h"

#include<string>
#include<chrono>

namespace muduo {

class Buffer;

// 单条 TCP 连接上的 HTTP/1.x 增量解析状态；可跨多次读事件继续解析。
class HttpContext {
public:
    enum class ParseState {
        RequestLine,
        Headers,
        Body,
        Done,
    };

    // 消费缓冲区中的完整行；数据不完整时保留状态并返回 true，格式错误返回 false。
    bool parseRequest(Buffer* buf);
    bool gotAll() const { return state_ == ParseState::Done; }
    const HttpRequest& request() const { return request_; }
    void reset() {
        state_ = ParseState::RequestLine;
        request_.reset();
        requestStartTime = std::chrono::steady_clock::now();
    }

    // 由 HttpServer 用于限制一个请求从开始接收到解析完成的时间。
    std::chrono::steady_clock::time_point requestStartTime{std::chrono::steady_clock::now()};

private:
    bool parseRequestLine(const char* begin, const char* end);

    ParseState state_{ParseState::RequestLine};
    HttpRequest request_;
};

} // namespace muduo
