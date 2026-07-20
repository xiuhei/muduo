#include"http/HttpContext.h"
#include"net/tcp/Buffer.h"

#include<algorithm>

namespace muduo{


bool HttpContext::parseRequestLine(const char* begin, const char* end) {
    // 找方法和路径之间的空格
    const char* space = std::find(begin, end, ' ');
    if (space == end) return false;

    if (!request_.setMethod(std::string_view(begin, space - begin))) {
        return false;
    }
// 找路径和版本之间的空格
    begin = space + 1;
    space = std::find(begin, end, ' ');
    if (space == end) return false;
    
    request_.setPath(std::string(begin, space));

    // 解析版本
    begin = space + 1;
    std::string_view version(begin, end - begin);
    if (version == "HTTP/1.1") {
        request_.setVersion(HttpRequest::Version::Http11);
    } else if (version == "HTTP/1.0") {
        request_.setVersion(HttpRequest::Version::Http10);
    } else {
        return false;
    }
    
    return true;

}


bool HttpContext::parseRequest(Buffer* buf){
    // Buffer 可能只含半行数据，因此解析器只在找到 CRLF 后推进状态和读索引。
    while(buf->readableBytes()>0){
    
        auto crlf=buf->findCRLF();
        if(crlf==nullptr)  return true;

        if(state_==ParseState::RequestLine){
            if (!parseRequestLine(buf->peek(), crlf)) return false; 
            buf->retrieve(crlf - buf->peek() + 2);
            state_ = ParseState::Headers;

        }else if (state_ == ParseState::Headers){
            if (crlf == buf->peek()) {
                // 遇到空行 \r\n，header 结束
                buf->retrieve(2);
                state_ = ParseState::Done; // GET 没有 body，直接 Done
            }else{
                // 解析一行 header：找冒号分割 field 和 value
                const char* colon = std::find(buf->peek(), crlf, ':');
                if (colon == crlf) return false;
                
                std::string field(buf->peek(), colon);
                const char* valueBegin = colon + 1;
                while (valueBegin < crlf && (*valueBegin == ' ' || *valueBegin == '\t')) ++valueBegin;
                std::string value(valueBegin, crlf);
                request_.addHeader(std::move(field), std::move(value));
                
                buf->retrieve(crlf - buf->peek() + 2);
            }
        }else if (state_ == ParseState::Done) {
            break;
        }

    }
    return true;
}

}//namespace muduo
