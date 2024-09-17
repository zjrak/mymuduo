#include "Poller.h"
#include "Channel.h"
Poller::Poller(EventLoop *Loop)
    :ownerLoop_(Loop)
    {

    }

bool Poller::hasChannel(Channel *channel) const
{
    ChannelMap::const_iterator it = Channels_.find(channel->fd());
    // auto it = Channels_.find(channel->fd());

    return it != Channels_.end() && it->second == channel;
}

