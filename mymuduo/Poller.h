#pragma once

#include "nocopyable.h"
#include "Timestamp.h"
#include <vector>
#include <unordered_map>
//muduo 库中的多路事件分发器的核心IO复用模块
class Channel;
class EventLoop;

//Base class for IO Multiplexing
//This class doesn't own the Channel objects.
class Poller :nocopyable
{
public:
    using ChannelList = std::vector<Channel*>;
    Poller(EventLoop *Loop);
    virtual ~Poller() = default;

    //给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;
    //判断参数channel是个否在当前的poller中
    virtual bool hasChannel(Channel *channel) const;
    //EventLoop可以通过该接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop *loop);

protected:
    //map的key：socked   value：sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap Channels_;

private:
    EventLoop *ownerLoop_;

};