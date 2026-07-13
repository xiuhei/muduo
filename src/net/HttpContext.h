#pragma once
#include "net/HttpRequest.h"

#include<string>
#include<chrono>

namespace muduo {

class Buffer;

class HttpContext {
public:
    enum class ParseState {
        RequestLine,
        Headers,
        Body,
        Done,
    };

    bool parseRequest(Buffer* buf);
    bool gotAll() const { return state_ == ParseState::Done; }
    const HttpRequest& request() const { return request_; }
    void reset() {
        state_ = ParseState::RequestLine;
        request_.reset();
        requestStartTime = std::chrono::steady_clock::now();
    }

    std::chrono::steady_clock::time_point requestStartTime{std::chrono::steady_clock::now()};

private:
    bool parseRequestLine(const char* begin, const char* end);

    ParseState state_{ParseState::RequestLine};
    HttpRequest request_;
};

} // namespace muduo