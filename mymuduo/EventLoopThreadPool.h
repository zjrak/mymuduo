#pragma once
#include "nocopyable.h"

#include <functional>
#include <memory>
#include <vector>
#include <string>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : nocopyable
{

public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    EventLoopThreadPool(EventLoop *baseLoop, const std::string &name);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) {numThreads = numThreads;}

    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    //如果工作在多线程中， baseLoop默认以轮询方式分配channel给subloop
    EventLoop *getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const {return started_;}
    const std::string name() const {return name_;}
private:

    EventLoop *baseLoop_;   //Eventloop loop;
    std::string name_;
    bool started_;
    int numThreads;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};