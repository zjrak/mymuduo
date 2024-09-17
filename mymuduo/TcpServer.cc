#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"


#include <functional>
#include <stdio.h>
#include <string>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/uio.h>  // readv
#include <unistd.h>

static EventLoop* CheckNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("mailloop is nuln");
    }
    return loop;
}

TcpServer::TcpServer(EventLoop* loop,
        const InetAddress& listenAddr,
        const std::string& nameArg,
        Option option)
        :loop_(CheckNotNull(loop))
        ,ipPort_(listenAddr.toIpPort())
        ,name_(nameArg)
        ,acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
        ,threadPool_(new EventLoopThreadPool(loop, name_))
        ,connectionCallback_()
        ,messageCallback_()
        ,nextConnId_(1)
        ,started_(0)
{
    acceptor_->setNewConnectionCallBack(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2)
        );
}

TcpServer::~TcpServer()
{
    for (auto& item : connections_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        conn->getLoop()->runInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn));
    }
}


void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{

    if (started_++ == 0) //防止一个TcpServer对象被start多次
    {
        threadPool_->start(threadInitCallback_); //启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// struct sockaddr* sockaddr_cast(struct sockaddr_in6* addr)
// {
//   return static_cast<struct sockaddr*>(implicit_cast<void*>(addr));
// }

/// 有一个新的客户端连接，会执行这个操作
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    EventLoop* ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] new connection [%s] from [%s]\n",
            name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    sockaddr_in localaddr;
    bzero(&localaddr, sizeof localaddr);

    socklen_t addrlen = static_cast<socklen_t>(sizeof localaddr);

    if (::getsockname(sockfd, (sockaddr*)&localaddr, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }

    InetAddress localAddr(localaddr);

    TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));
    connections_[connName] = conn;
    //下面的回调都是用户设置给TcpServer=>TcpConnection=>Channel=>Poller notify channel调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);


    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)); // FIXME: unsafe

    
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}
/// Thread safe.
void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}
/// Not thread safe, but in loop
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop[%s] - connection[%s]", name_.c_str(), conn->name().c_str());

    size_t n = connections_.erase(conn->name());
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
      std::bind(&TcpConnection::connectDestroyed, conn));
}
