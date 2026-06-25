#include"net/HttpResponse.h"
#include"net/Buffer.h"

#include<cstdio>

namespace muduo{

void HttpResponse::appendToBuffer(Buffer* buf) const {
    // 状态行
    char statusLine[64];
    snprintf(statusLine, sizeof(statusLine),
             "HTTP/1.1 %d %s\r\n",
             static_cast<int>(statusCode_),
             statusMessage_.c_str());
    buf->append(statusLine);

    // headers
    if (closeConnection_) {
        buf->append("Connection: close\r\n");
    } else {
        // 告诉客户端 body 有多长，keep-alive 必须有这个
        char contentLength[64];
        snprintf(contentLength, sizeof(contentLength),
                 "Content-Length: %zd\r\n", body_.size());
        buf->append(contentLength);
        buf->append("Connection: keep-alive\r\n");
    }

    for (const auto& [field, value] : headers_) {
        buf->append(field + ": " + value + "\r\n");
    }

    buf->append("\r\n"); // 空行，header 结束
    buf->append(body_);
}

}

