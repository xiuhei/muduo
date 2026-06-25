#pragma once

#include<string>
#include<map>


namespace muduo{

class HttpRequest{
public:    
    enum Method {GET,POST,HEAD,UNKNOWN};
    enum class Version { Unknown, Http10, Http11 };

    void reset();


    //get
    Method method() const { return method_; }
    Version version() const { return version_; }
    const std::string& path() const { return path_; }

    std::string getHeader(const std::string& field) const {
        auto it = headers_.find(field);
        return it != headers_.end() ? it->second : "";
    }
    const std::map<std::string, std::string>& headers() const { return headers_; }

    //set
    bool setMethod(std::string_view method);
    void setVersion(Version version) { version_ = version; }
    void setPath(std::string path) { path_ = std::move(path); }
    void setQuery(std::string query) { path_ = std::move(query); }

    void addHeader(std::string filed,std::string value);
    
private:
    Method method_{UNKNOWN};    //GET/POST
    std::string path_{};          //index.html
    std::string query_{};         //?...
    Version version_{};       //HTTP1.1
    std::map<std::string , std::string> headers_{};
};




}//namespace muduo