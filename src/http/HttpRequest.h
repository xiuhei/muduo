#pragma once

#include<string>
#include<map>


namespace muduo{

// HTTP 请求的解析结果，仅保存协议层字段，不持有连接或输入缓冲区。
class HttpRequest{
public:    
    enum Method {GET,POST,HEAD,UNKNOWN};
    enum class Version { Unknown, Http10, Http11 };

    void reset();


    Method method() const { return method_; }
    Version version() const { return version_; }
    const std::string& path() const { return path_; }

    // HTTP 字段名大小写不敏感；不存在时返回空字符串。
    std::string getHeader(const std::string& field) const;
    const std::map<std::string, std::string>& headers() const { return headers_; }

    // 校验并设置当前支持的方法，未知方法返回 false。
    bool setMethod(std::string_view method);
    void setVersion(Version version) { version_ = version; }
    void setPath(std::string path) { path_ = std::move(path); }
    void setQuery(std::string query) { path_ = std::move(query); }

    void addHeader(std::string filed,std::string value);
    
private:
    Method method_{UNKNOWN};
    std::string path_{};
    std::string query_{};
    Version version_{};
    std::map<std::string , std::string> headers_{};
};




}//namespace muduo
