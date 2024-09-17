#pragma once
#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include "nocopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"
/* 
时间循环类 包含 channl poller（epoll的抽象） */

// Reactor

class Channel;
class Poller;

class EventLoop :nocopyable
{
public:
    using  Functor = std::function<void()>;
    EventLoop();

    ~EventLoop();

    //开启事件循环
    void loop();
    //退出事件循环
    void quit();

    Timestamp pollReturnTime() const {return pollReturnTime_;}

    //在当前loop中执行cb
    void runInLoop(Functor cb);

    //把cb放入队列中，唤醒loop所在的线程中 执行cb
    void queueInLoop(Functor cb);

    //唤醒loop所在的线程
    void wakeup();

    //EventLoop的方法=》poller方法
    void updateChannel(Channel *channel);

    void removeChannel(Channel *channel);

    bool hasChannel(Channel *channel);

    //判断EventLoop对象是否在自己的线程中
    bool isInLoopThread() const {return threadId_ == CurrentThread::tid();}




private: 

    // wake up
    void handleRead(); 
    //执行回调
    void doPendingFunctors();

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;    /* atomic 原子实现 */
    std::atomic_bool quit_;    //标志推出loop循环

    std::atomic_bool callingPendingFunctors_;  //标识当前loop是否有需要执行的回调操作

    const pid_t threadId_;  //记录当前loop所在的线程id

    Timestamp pollReturnTime_; //记录poll返回发生事件的channels的时间点

    std::unique_ptr<Poller> poller_;

    int wakeupFd_;   //当mainloop 获取一个新用户的channel ，通过轮询算法选择一个subloop  通过该成员唤醒subloop处理channel
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;
    Channel *currentActiveChannel_;

    std::vector<Functor> pendingFunctors_; //存储loop需要执行的所有回调操作

    std::mutex mutex_;  //互斥锁 用来保护上面vector容器的线程安全操作
};