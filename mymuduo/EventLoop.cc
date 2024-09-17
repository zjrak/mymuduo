#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"
#include <unistd.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

//防止一个线程创建多个EventLoop  thread_local
__thread EventLoop *t_loopInThisThread = nullptr;

//定义默认的Poller IO复用接口的超时时间
const int kPollTimes = 10000;

int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK|EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d\n", errno);
    }
    return evtfd;
}


EventLoop::EventLoop()
    :looping_(false)
    ,quit_(false)
    ,callingPendingFunctors_()
    ,threadId_(CurrentThread::tid())
    ,poller_(Poller::newDefaultPoller(this))
    ,wakeupFd_(createEventfd())
    ,wakeupChannel_(new Channel(this, wakeupFd_))
    ,currentActiveChannel_(nullptr)
{
    LOG_DEBUG("EventLoop create:%p in threan\n", this, threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }
    
    //设置wakeupfd的事件类型，以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    //每一个EventLoop都将监听wakeupchannel的EPOLLIN读事件
    wakeupChannel_->enableReading();
}
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
}

//开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping", this);

    while (!quit_)
    {
        activeChannels_.clear();
        // 监听两类fd  一种是client的fd  一种是wakeup的fd
        pollReturnTime_ = poller_->poll(kPollTimes, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {   
            LOG_INFO("EventLoop::loop: the channel fd:%d", channel->fd());
            //Poller监听哪些channel发生事件了，然后上报给EventLoop,通知channel负责调用回调操作
            channel->handleEvent(pollReturnTime_);
        }
        //执行当前EventLoop事件循环需要处理的回调操作
        /* 
        * IO线程    mainloop   accept新用户连接  返回fd被channel包装  =》subloop
        *mainloop 实现注册一个回调cd（需要subloop执行）  wakeup subloop后  执行下面的方法，执行之前mainloop注册的cb
        *  */
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping", this);
    looping_ = false;
}
//退出事件循环
// 1 loop在自己线程中调用quit
// 2 在其他线程中调用quit ：在一个subloop中调用mainloop的quit

/* 

                                mainloop
                    subloop1    subloop2     subloop3



 */
void EventLoop::quit()
{
    quit_ = true;
    if (!isInLoopThread())
    {
        wakeup();
    }
}


 //在当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    LOG_INFO("EventLoop::runInLoop\n");
    if (isInLoopThread()) //在当前loop线程中  执行cd
    {
        cb();
    }
    else //在非当前loop中执行cb，就需要唤醒loop所在线程，执行cb
    {
        queueInLoop(std::move(cb));
    }
}

//把cb放入队列中，唤醒loop所在的线程中 执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }

    //唤醒相应的，需要执行回调操作的loop线程，执行cb
    //callingPendingFunctors_  当前loop正在执行回调，但是loop又有了新的回调
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();  //唤醒loop所在线程
    }
}

//唤醒loop所在的线程
//向wakeupfd写入数据,wakefdChannel就发生读事件。线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup()writes %lu bytes instead of 8 \n", n);
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() Reads %d bytes instead of 8", n);
    }
}

//EventLoop的方法=》poller方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}


void  EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor &functor : functors)
    {
        functor(); //执行当前loop需要执行的回调
    }
    callingPendingFunctors_ = false;
}