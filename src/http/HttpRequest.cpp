#include "http/HttpRequest.h"

#include <algorithm>
#include <cctype>

namespace muduo{

void HttpRequest::reset(){
    method_=Method::UNKNOWN;
    path_.clear();
    version_=Version::Unknown;
    headers_.clear();
    
}

bool HttpRequest::setMethod(std::string_view method) {
    if (method == "GET") {
        method_ = Method::GET;
    } else if (method == "POST") {
        method_ = Method::POST;
    } else if (method == "HEAD") {
        method_ = Method::HEAD;
    }else {
        method_ = Method::UNKNOWN;
    }
    return method_ != Method::UNKNOWN;
}

void HttpRequest::addHeader(std::string filed,std::string value){
    headers_[std::move(filed)]=std::move(value);
    
}

std::string HttpRequest::getHeader(const std::string& field) const {
    // map 默认区分大小写，这里按 HTTP 语义逐项执行大小写无关匹配。
    auto equalIgnoreCase = [](const std::string& lhs, const std::string& rhs) {
        return lhs.size() == rhs.size() &&
               std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                          [](unsigned char a, unsigned char b) {
                              return std::tolower(a) == std::tolower(b);
                          });
    };
    for (const auto& header : headers_) {
        if (equalIgnoreCase(header.first, field)) return header.second;
    }
    return {};
}


}//namespace muduo
