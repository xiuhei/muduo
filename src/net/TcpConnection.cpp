#include "net/TcpConnection.h"
#include "net/Channel.h"
#include "net/EventLoop.h"

#include<unistd.h>
#include <cerrno>
#include <cstring>
#include<iostream>
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
    
void TcpConnection::connectEstablished(){
loop_->assertInLoopThread();
    setState(State::Connected);
    // 连接建立后才把 connfd 加入 epoll，之后读事件由所属 IO loop 处理。
    channel_.enableReading();
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
}

void TcpConnection::handleRead() {
    while(true){
        int savedErrno = 0;
        // 非阻塞 fd 要一直读到 EAGAIN，避免本轮可读事件的数据没取干净。
        ssize_t n = inputBuffer_.readFd(socket_.fd(), &savedErrno);
        if (n > 0) {
            if (messageCallback_) {
                messageCallback_(shared_from_this(), &inputBuffer_);
            }
            continue;
        }
        if (n == 0) {
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
    } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        handleError();
        return;
    }

    if(outputBuffer_.readableBytes()==0){
        channel_.disableWriting();
        if(state_==State::Disconnecting){
            shutdownInLoop();
        }
    }
}

void TcpConnection::handleClose() {
    loop_->assertInLoopThread();
    setState(State::Disconnected);
    channel_.disableAll();
    if (closeCallback_) {
        closeCallback_(shared_from_this());
    }
}

void TcpConnection::handleError() {
    std::cerr << "TcpConnection error on " << name_ << ": " << std::strerror(errno) << '\n';
}

void TcpConnection::shutdownInLoop() {
    loop_->assertInLoopThread();
    if (!channel_.isWriting()) {
        socket_.shutdownWrite();
    }
}

void TcpConnection::send(std::string message) {
    if (state_ != State::Connected) {
        return;
    }
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
        return;
    }

    if (!channel_.isWriting() && outputBuffer_.readableBytes() == 0) {
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
    if (!channel_.isWriting()) {
        channel_.enableWriting();
    }
}

}// namespace muduo
