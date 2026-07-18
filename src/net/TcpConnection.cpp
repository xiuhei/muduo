#include "net/TcpConnection.h"
#include "base/Logger.h"
#include "net/Channel.h"
#include "net/EventLoop.h"

#include<unistd.h>
#include <cerrno>
#include <cstring>
#include <utility>

namespace muduo
{

TcpConnection::TcpConnection(EventLoop* loop,std::string name, int sockfd, const InetAddress& localAddr, const InetAddress& peerAddr)
    : loop_(loop),
      name_(std::move(name)),
      socket_(sockfd),
      channel_(loop, sockfd),
      localAddr_(localAddr),
      peerAddr_(peerAddr) {
    channel_.setReadCallback([this] { handleRead(); });
    channel_.setWriteCallback([this] { handleWrite(); });
    channel_.setCloseCallback([this] { handleClose(); });
    channel_.setErrorCallback([this] { handleError(); });
      }

    
TcpConnection::~TcpConnection() = default;


bool TcpConnection::isIdleTimeout(std::chrono::steady_clock::time_point now) const{
    if(idleTimeout_.count()==0){ return false;}
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastActiveTime_);
    return elapsed >= idleTimeout_;
}

bool TcpConnection::isWriteTimeout(TimePoint now) const {
    if (writeTimeout_.count() == 0) return false;
    if (writeStartTime_ == TimePoint{}) return false;  // 没有积压数据
    return (now - writeStartTime_) > writeTimeout_;
}
    
void TcpConnection::connectEstablished(){
    loop_->assertInLoopThread();
    setState(State::Connected);
    // 连接建立后才把 connfd 加入 epoll，之后读事件由所属 IO loop 处理。
    channel_.enableReading();
    lastActiveTime_ = std::chrono::steady_clock::now();
    LOG_DEBUG("connection established: {} local={} peer={}",
              name_, localAddr_.toIpPort(), peerAddr_.toIpPort());
    if (connectionCallback_) {
        connectionCallback_(shared_from_this());
    }
}

void TcpConnection::connectDestroyed(){
    loop_->assertInLoopThread();
    if (state_ == State::Connected) {
        setState(State::Disconnected);
        channel_.disableAll();
        if (connectionCallback_) {
            connectionCallback_(shared_from_this());
        }
    }
    loop_->removeChannel(&channel_);
    LOG_DEBUG("connection destroyed: {}", name_);
}

void TcpConnection::handleRead() {
    while(true){
        int savedErrno = 0;
        // 非阻塞 fd 要一直读到 EAGAIN，避免本轮可读事件的数据没取干净。
        ssize_t n = inputBuffer_.readFd(socket_.fd(), &savedErrno);
        lastActiveTime_ = std::chrono::steady_clock::now();
        if (n > 0) {
            if (messageCallback_) {
                messageCallback_(shared_from_this(), &inputBuffer_);
            }
            continue;
        }
        if (n == 0) {
            LOG_DEBUG("peer closed connection: {}", name_);
            handleClose();
            break;
        }
        if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK) {
            break;
        }
        if (savedErrno == EINTR) {
            continue;
        }
        errno = savedErrno;
        handleError();
        break;
    }
}


void TcpConnection::handleWrite() {
    if(!channel_.isWriting()){
        return;
    }
    ssize_t n=::write(socket_.fd(),outputBuffer_.peek(),outputBuffer_.readableBytes());
    if (n > 0) {
        outputBuffer_.retrieve(static_cast<size_t>(n));
        // 每次成功写出数据时更新最后活跃时间戳
        lastActiveTime_ = std::chrono::steady_clock::now();
    } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        handleError();
        return;
    }

    if(outputBuffer_.readableBytes()==0){
        channel_.disableWriting();
        // 缓冲区清空，清除写入开始时间
        writeStartTime_ = TimePoint{};
        if(state_==State::Disconnecting){
            shutdownInLoop();
        }
    }
}

void TcpConnection::handleClose() {
    loop_->assertInLoopThread();
    loop_->cancel(shutdownTimerId_);
    setState(State::Disconnected);
    channel_.disableAll();
    LOG_INFO("connection closed: {} peer={}", name_, peerAddr_.toIpPort());
    if (closeCallback_) {
        closeCallback_(shared_from_this());
    }
}

void TcpConnection::handleError() {
    LOG_ERROR("TcpConnection error on {}: {}", name_, std::strerror(errno));
}

void TcpConnection::shutdownInLoop() {
    loop_->assertInLoopThread();
    if (!channel_.isWriting()) {
        socket_.shutdownWrite();
    }
    // 设置优雅关闭超时：如果对端在 shutdownTimeout_ 时间内不回 FIN，
    // 则强制关闭连接，防止连接永久卡在 Disconnecting 状态
    if (shutdownTimeout_.count() > 0) {
        shutdownTimerId_ = loop_->runAfter(shutdownTimeout_, [self = shared_from_this()] {
            self->forceCloseInLoop();
        });
    }
}

void TcpConnection::send(std::string message) {
    if (loop_->isInLoopThread()) {
        sendInLoop(std::move(message));
    } else {
        // outputBuffer_ 只在所属 IO 线程中修改，跨线程发送要投递回 loop。
        loop_->queueInLoop([self = shared_from_this(), msg = std::move(message)]() mutable {
            self->sendInLoop(std::move(msg));
        });
    }
}

void TcpConnection::sendInLoop(std::string message) {
    loop_->assertInLoopThread();
    if (state_ == State::Disconnected) {
        LOG_WARN("discarding {} bytes for disconnected connection {}",
                 message.size(), name_);
        return;
    }

    bool bufferWasEmpty = (outputBuffer_.readableBytes() == 0);

    if (!channel_.isWriting() && bufferWasEmpty) {
        // 先尝试直接写；写不完的部分再进入输出缓冲区，等待 EPOLLOUT。
        ssize_t n = ::write(socket_.fd(), message.data(), message.size());
        if (n >= 0) {
            message.erase(0, static_cast<size_t>(n));
            if (message.empty()) {
                return;
            }
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            handleError();
            return;
        }
    }

    outputBuffer_.append(message);
    // 缓冲区从空变为非空时记录写入开始时间，用于写入超时检测
    if (bufferWasEmpty && writeTimeout_.count() > 0) {
        writeStartTime_ = std::chrono::steady_clock::now();
    }
    if (!channel_.isWriting()) {
        channel_.enableWriting();
    }
}

void TcpConnection::shutdown() {
    auto self = shared_from_this();
    loop_->runInLoop([self] {
        if (self->state_ != State::Connected) return;
        LOG_DEBUG("graceful shutdown requested for connection {}", self->name_);
        self->setState(State::Disconnecting);
        self->shutdownInLoop();
    });
}

void TcpConnection::forceClose() {
    if (loop_->isInLoopThread()) {
        forceCloseInLoop();
    } else {
        loop_->queueInLoop([self = shared_from_this()] { self->forceCloseInLoop(); });
    }
}

void TcpConnection::forceCloseInLoop() {
    loop_->assertInLoopThread();
    if (state_ != State::Disconnected) {
        LOG_WARN("force closing connection {}", name_);
        handleClose();
    }
}

}// namespace muduo
