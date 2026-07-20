#pragma once

#include "base/NonCopyable.h"

#include <sys/epoll.h>
#include <vector>
#include <unordered_map>

namespace muduo
{

class Channel;
class EventLoop;

// epoll 的最小封装，维护 fd 到 Channel 的注册关系和注册状态机。
class Poller : public NonCopyable {

public:
    Poller();
    ~Poller();

    // 等待事件并把活跃 Channel 追加到 activeChannels。
    void poll(int timeoutMs, std::vector<Channel*>* activeChannels);
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

private:
    static constexpr int kNew = -1;
    static constexpr int kAdded = 1;
    static constexpr int kDeleted = 2;

    void update(int operation, Channel* channel);

    int epollfd_{-1};
    std::vector<epoll_event> events_;
    std::unordered_map<int, Channel*> channels_;

    void fillActiveChannels(int numEvents, std::vector<Channel*>* activeChannels) const;
    

};

} // namespace muduo
