#pragma once
#include "nocopyable.h"
#include "Timestamp.h"
#include <functional>
#include <memory>
#include <stdio.h>
/* 

理清楚 EventLoop Channele， Poller之间的关系 《= Reactor模型上对应的Demultiplex

理解为通道  封装了sockfd和其感兴趣的event 如EPOLLIN EPOLLOUT 事件 
还绑定了poller返回的具体事件*/

//类型前置声明
class EventLoop;
class Timestamp;

class Channel :nocopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *Loop, int fd);
    ~Channel();

    // fd得到poller通知以后，处理事件的
    void handleEvent(Timestamp receiveTime);
    //设置回调函数对象
    void setReadCallback(ReadEventCallback cb) {readCallback_ = std::move(cb);}
    void setWriteCallback(EventCallback cb) {writeCallback_ = std::move(cb);}
    void setCloseCallback(EventCallback cb){closeCallback_ = std::move(cb);}
    void setErrorCallback(EventCallback cb) {errorCallback_ = std::move(cb);}

    //防止Channel被手动remove之后，Channel还在执行回调操作  
    void tie(const std::shared_ptr<void>&);

    int fd() const {return fd_;}
    int events() const {return events_;}
    void set_revents(int revt) {revents_ = revt;}
    

    //设置fd相应的事件状态
    void enableReading() {printf("enableReading\n");events_ |= kReadEvent; update();}

    void disableReading() {printf("disableReading\n");events_ &= ~kReadEvent; update();}

    void enableWriting() {printf("enableWriting\n");events_ |= kWriteEvent; update();}
    void disableWriting() {printf("disableWriting\n");events_ &= ~kWriteEvent; update();}

    void disableAll() {printf("disableAll\n");events_ = kNoneEvent; update();}

    //返回fd当前事件状态

    bool isNoneEvent() const {return events_ == kNoneEvent;}

    bool isWriting() const {return events_ == kWriteEvent;}

    bool isReading() const {return events_ == kReadEvent;}

    int index() {return index_;}

    void set_index(int idx) {index_ = idx;}


    //one loop per thread

    EventLoop *ownerLoop() {return loop_;}

    void remove();

private:

    void update();
    void handleEventWhitGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_; //事件循环
    const int fd_;   //fd poller监听的事件
    int events_;   //注册fd感兴趣的事件
    int revents_; //poller返回具体发生的事件
    int index_;

    std::weak_ptr<void> tie_;
    bool tied_;

    //因为Channel通道里面能够获知fd最终发生具体的事件revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};