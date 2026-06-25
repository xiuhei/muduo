#pragma once

#include<map>
#include<string>

namespace muduo{

class Buffer;

class HttpResponse{
public:
    enum class StatusCode {
        Ok = 200,
        BadRequest = 400,
        NotFound = 404,
        MethodNotAllowed = 405,
        InternalServerError = 500,
    };

    //get
    bool closeConnection() const { return closeConnection_; }

    //set
    void setStatusCode(StatusCode code) { statusCode_ = code; }
    void setStatusMessage(std::string message) { statusMessage_ = std::move(message); }
    void setCloseConnection(bool on) { closeConnection_ = on; }
    void setBody(std::string body) { body_ = std::move(body); }
    void setContentType(std::string_view type) {
        headers_["Content-Type"] = std::string(type);
    }
    void addHeader(std::string field, std::string value){
        headers_[std::move(field)]=std::move(value);
    }

    //序列化进入buffer
    void appendToBuffer(Buffer* buf) const;
private:
    StatusCode statusCode_{StatusCode::Ok};
    std::string statusMessage_{};
    bool closeConnection_{false};
    std::map<std::string, std::string> headers_;
    std::string body_{};

};


}//namespace muduo