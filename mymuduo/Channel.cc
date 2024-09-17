#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *Loop, int fd)
    : loop_(Loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}
Channel::~Channel()
{
    // if (loop->isInLoopThread()) {
    //     assert(!loop->hasChannel());
    // }
}

//一个TcpConnection新连接创建的时候，TcpConnection=》channel
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}


/* 
* 当改变Channel所表示fd的事件后 update负责在poller里面改变fd相应的事件epoll_ctl
*EventLoop : ChannelList   Poller
 */
void Channel::update()
{
    //通过Channel所属的EventLoop，调用poller的相应方法，注册fd的event事件
    LOG_INFO("func= Channel::update fd");
    //add code...
    // loop->updateChannel(this)

    // addedToLoop_ = true;
    loop_->updateChannel(this);

}

// 在channel 所属的EventLoop中，把当前的channel删除
void Channel::remove()
{

    //add code...
    // loop->removeChannel();
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    std::shared_ptr<void> guard;
    if (tied_)
    {
        guard = tie_.lock();
        if (guard) {
            handleEventWhitGuard(receiveTime);
        }
    } else 
    {
        handleEventWhitGuard(receiveTime);
    }
}


// 根据poller通知的channel发生的具体事件 由channel负责调用具体的回调操作
void Channel::handleEventWhitGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d\n", revents_);
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        closeCallback_();
    }

    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if (revents_ & EPOLLOUT) 
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}