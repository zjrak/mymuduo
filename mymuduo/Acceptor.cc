#include "Acceptor.h"
#include "Logger.h"
#include "Socket.h"
#include "InetAddress.h"
#include "Channel.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
 #include <unistd.h>

static int createNonblockingOrDie()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK |SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d, listen socket create error:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    :loop_(loop)
    ,acceptSocket_(createNonblockingOrDie())
    ,acceptChannel_(loop, acceptSocket_.fd())
    ,listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);

    //Tcpserver::start()  Acceptor.listen()  有新用户连接，要执行一个回调  (connfd => channel => subloop)
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}
Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}
void Acceptor::listen()
{
    LOG_INFO("Acceptor::listen\n");
    listenning_ = true;
    acceptSocket_.listen();

    acceptChannel_.enableReading();
}

// listenfd 有事件发生了，就是有新用户连接了
void Acceptor::handleRead()
{
    InetAddress peerAddr;

    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        if (NewConnectionCallback_)
        {
            NewConnectionCallback_(connfd, peerAddr); //轮询找到subloop。唤醒，分发当前的新客户端的Channel
        }
        else 
        {
            ::close(connfd);
        }
    } 
    else 
    {
        LOG_ERROR("%s:%s:%d, accept error:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d, sockfd reached limit error:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
        }
    }
}
