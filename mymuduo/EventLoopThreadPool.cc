#include "EventLoopThreadPool.h"
#include <memory>
#include "EventLoop.h"
#include "EventLoopThread.h"


EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &name)
    :baseLoop_(baseLoop)
    ,name_(name)
    ,started_(false)
    ,numThreads(0)
    ,next_(0)
{

}
EventLoopThreadPool::~EventLoopThreadPool()
{

}


void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;
    for (int i = 0; i < numThreads; ++i)
    {
        char buf[name_.size()+32];

        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop());
    }
    //整个服务只有一个线程
    if (numThreads == 0 &&cb)
    {
        cb(baseLoop_);
    }
}

//如果工作在多线程中， baseLoop默认以轮询方式分配channel给subloop
EventLoop * EventLoopThreadPool::getNextLoop()
{
    EventLoop *loop = baseLoop_;

    if (!loops_.empty())  //通过轮询获取处理下一个处理事件的loop
    {
        loop = loops_[next_];
        ++next_;

        if (next_ >= loops_.size())
        {
            next_ = 0;
        }
    }
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty())
    {
        return std::vector<EventLoop*>(1, baseLoop_);
    }
    else {
        return loops_;
    }
}
