#include "net/HttpRequest.h"

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


}//namespace muduo