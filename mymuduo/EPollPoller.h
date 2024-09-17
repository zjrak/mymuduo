#pragma once

#include "nocopyable.h"
#include "Poller.h"
#include "Timestamp.h"
#include <vector>
#include <sys/epoll.h>


/* 
epoll 的使用
epoll_create
epoll_ctl  add/modify/del
epoll_wait

 */
class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    //重写基类poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelList *avtiveChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;
private:
    static const int kInitEventListSize = 16;

    //填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    //更新channel通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};