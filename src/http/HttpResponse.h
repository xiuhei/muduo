#pragma once

#include<map>
#include<string>

namespace muduo{

class Buffer;

// 由业务回调填充的 HTTP 响应模型，最终序列化到 TCP Buffer。
class HttpResponse{
public:
    enum class StatusCode {
        Ok = 200,
        BadRequest = 400,
        NotFound = 404,
        MethodNotAllowed = 405,
        InternalServerError = 500,
    };

    bool closeConnection() const { return closeConnection_; }

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

    // 写入完整状态行、响应头、空行和正文；不负责实际网络发送。
    void appendToBuffer(Buffer* buf) const;
private:
    StatusCode statusCode_{StatusCode::Ok};
    std::string statusMessage_{};
    bool closeConnection_{false};
    std::map<std::string, std::string> headers_;
    std::string body_{};

};


}//namespace muduo
