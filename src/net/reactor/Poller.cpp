#include "net/reactor/Poller.h"
#include "net/reactor/Channel.h"
#include "base/Logger.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>

namespace muduo
{

Poller::Poller() 
    : epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
        events_(16){
        if (epollfd_ < 0) {
            LOG_ERROR("Poller::Poller: {}", std::strerror(errno));
        }
}

Poller::~Poller() {
    ::close(epollfd_);
}


void Poller::poll(int timeoutMs, std::vector<Channel*>* activeChannels) {
    int numEvents = ::epoll_wait(epollfd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);
    if (numEvents > 0) {
        fillActiveChannels(numEvents, activeChannels);
        if (static_cast<size_t>(numEvents) == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (numEvents < 0 && errno != EINTR) {
        LOG_ERROR("Poller::poll: {}", std::strerror(errno));
    }
}

void Poller::update(int operation, Channel* channel) {
    struct epoll_event event{};
    event.events = channel->events();
    event.data.ptr = channel;
    if(::epoll_ctl(epollfd_, operation, channel->fd(), &event) < 0) {
        LOG_ERROR("Poller::update: {}", std::strerror(errno));
    }
}

void Poller::fillActiveChannels(int numEvents, std::vector<Channel*>* activeChannels) const {
    for (int i = 0; i < numEvents; ++i) {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->setRevents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

void Poller::updateChannel(Channel* channel) {
    const int index = channel->index();
    // kDeleted 仍保留在 channels_ 中，再次关注事件时应重新 ADD。
    if (index == kNew || index == kDeleted) {
        if (index == kNew) {
            channels_[channel->fd()] = channel;
        }
        update(EPOLL_CTL_ADD, channel);
        channel->setIndex(kAdded);
    } else {
        if (channel->isNoneEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->setIndex(kDeleted);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void Poller::removeChannel(Channel* channel) {
    // remove 表示生命周期结束；不同于 disableAll，连 fd 到 Channel 的映射也删除。
    int fd = channel->fd();
    channels_.erase(fd);
    if (channel->index() == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->setIndex(kNew);
}

} // namespace muduo
