#include "EPollPoller.h"
#include "Logger.h"
#include "errno.h"
#include "Channel.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>
//channel未添加到poller中
const int kNew = -1;  //channel index_ = -1  
//channel已添加到poller中
const int kAdded = 1;
//channel从poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)
    :Poller(loop) , epollfd_(::epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize)
{
    if (epollfd_ < 0) 
    {
        LOG_FATAL("epoll_create error:%d\n", errno);
    }
}
EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}


//填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        //EventLoop 拿到了它的poller给他返回的所有发生事件的channel列表
        activeChannels->push_back(channel);  
    }
}

//wait
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *avtiveChannels)
{
    //用LOG_DEBUG 合适
    LOG_INFO("func= poll fd total count:%lu",  Channels_.size());

    int numEvents = ::epoll_wait(epollfd_, 
                                &*events_.begin(),
                                static_cast<int>(events_.size()),
                                timeoutMs);

    int saveError = errno;

    Timestamp now(Timestamp::now());
    if (numEvents > 0)
    {
        LOG_INFO("%d events happened\n", numEvents);
        fillActiveChannels(numEvents, avtiveChannels);
        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("fun = poll timeout \n");
    }
    else
    {
        if (saveError != EINTR)
        {
            errno = saveError;
            LOG_ERROR("EPillPoller::poll() error!");
        }
    }
    return now;
}

//channel update remove-> 
//EventLoop  updateChannel removeChannel -> 
//Poller updateChannel removeChannel


/* 
*                         EventLoop =》poller.poll
*   ChannelList:所有的channel            Poller
                                    ChannelMap <fd, channel*> 在poller中注册的poller
 */


void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("func= updateChannel fd=%d events=%d index=%d", channel->fd(), channel->events(), index);
    if (index == kNew || index == kDeleted)
    {
        int fd = channel->fd();
        if (index == kNew)
        {
            Channels_[fd] = channel;
        } 
        else
        {

        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);

    }
    else //channel已经在poller上注册过了
    {
        int fd = channel->fd();
        // assert()

        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
        }
        else 
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
    
}    

//从poller中删除channel
void EPollPoller::removeChannel(Channel *channel)
{
    LOG_INFO("func = removeChannel fd=%d events=%d ", channel->fd(), channel->events());
    int fd = channel->fd();
    size_t n = Channels_.erase(fd);

    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }

    channel->set_index(kNew);
}



//更新channel通道
void EPollPoller::update(int operation, Channel *channel)
{
    struct epoll_event event;
    memset(&event, 0, sizeof event);
    int fd = channel->fd();

    event.events = channel->events();

    event.data.fd = fd;

    event.data.ptr = channel;
    LOG_INFO("EPollPoller::update fd %d\n", fd);
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl del/mod error:%d\n", errno);
        }
    }
}