#include "Poller.h"
#include "EPollPoller.h"
#include <stdlib.h>
// 此文件为了实现 抽象父类不包含基类的头文件 
// 实现抽象基类的函数

Poller* Poller::newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        return nullptr; //生成poll的实列
    }
    else
    {
        return new EPollPoller(loop);  //生成epoll实列
    }
}