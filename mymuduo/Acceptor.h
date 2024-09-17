#pragma once
#include "Channel.h"
#include "Socket.h"
#include "nocopyable.h"

#include <functional>
#include <sys/types.h>
#include <sys/socket.h>

class EventLoop;
class InetAddress;

class Acceptor : nocopyable
{
public:
    using NewConnectionCallback =  std::function<void (int sockfd, const InetAddress&)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallBack(const NewConnectionCallback &cb)
    {NewConnectionCallback_ = cb;}

    bool listenning() const {return listenning_;}
    void listen();
private:
    void handleRead();
    EventLoop *loop_; //Acceptor用的就是用户定义的那个baseloop,称作mainloop
    Socket acceptSocket_;
    Channel acceptChannel_;

    NewConnectionCallback NewConnectionCallback_;
    bool listenning_;

};