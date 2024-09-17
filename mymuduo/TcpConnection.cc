#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"


#include <errno.h>
#include <functional>
#include <sys/socket.h>
#include <sys/uio.h>  // readv
#include <unistd.h>
#include <string>

static EventLoop* CheckNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("mailloop is nuln");
    }
    return loop;
}


TcpConnection::TcpConnection(EventLoop* loop,
                             const std::string& nameArg,
                             int sockfd,
                             const InetAddress& localAddr,
                             const InetAddress& peerAddr)
  : loop_(CheckNotNull(loop)),
    name_(nameArg),
    state_(kConnecting),
    reading_(true),
    socket_(new Socket(sockfd)),
    channel_(new Channel(loop, sockfd)),
    localAddr_(localAddr),
    peerAddr_(peerAddr),
    highWaterMark_(64*1024*1024)
{
    //下面设置相应的回调函数，poller给channel通知感兴趣的事件发生了，channel会回调相应的操作函数
  channel_->setReadCallback(
      std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
  channel_->setWriteCallback(
      std::bind(&TcpConnection::handleWrite, this));
  channel_->setCloseCallback(
      std::bind(&TcpConnection::handleClose, this));
  channel_->setErrorCallback(
      std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor:[%s] at %d\n", name_.c_str(), sockfd);
  socket_->setKeepAlive(true);
}


TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s]=%d, state%d", name_.c_str(), channel_->fd(), (int)state_);
}

//发送数据
void TcpConnection::send(const std::string &buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else{
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,
                this,
                buf.c_str(),
                buf.size()
            ));
        }
    }
}

//发送数据
/* 

 */
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;
    //之前调用过该COnnection的shutdown，不能再进行发送
    if (state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing");
    }

    //表示channel_第一次开始写数据，而且缓冲区没有待发送数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0) {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_)
            {
                //既然在这里数据全部发送完成，就不用在给channel设置epollout事件了
                loop_->queueInLoop(std::bind(
                    writeCompleteCallback_, shared_from_this()
                ));
            }
        }
        else
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
       
    }
    //当前这一次write，并没有将数据全部发送出去，
    //剩余的数据需要保存到缓冲区中，然后给channel注册epollout事件
    //poller发现tcp的数据缓冲区有空间，会通知相应的sock-channel。调用writeCallback_回调方法
    //也就是调用TcpConnection::handleWrite的方法，把缓冲区的数据全部发送完成
    if (!faultError && remaining > 0) 
    {
        //
        size_t oldlen = outputBuffer_.readableBytes();
        if (oldlen + remaining >= highWaterMark_  && oldlen < highWaterMark_ && highWaterMark_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldlen + remaining)
            );

        }

        outputBuffer_.append((char*)data + nwrote, remaining);

        if (!channel_->isWriting())
        {
            channel_->enableWriting(); //注册channel的写事件，否词poller不会给channel通知epollout
        }
    }
}


//连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    LOG_INFO("TcpConnection::connectEstablished\n");
    channel_->enableReading(); //向poller注册channel的epollin事件

    //新连接建立
    //执行回调
    connectionCallback_(shared_from_this());
}

//连接销毁
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();

        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}

//关闭连接
void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        // FIXME: shared_from_this()?
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting())
    {
        // we are not writing
        socket_->shutdownWrite(); // 关闭写段
    }
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0)
    {   
        //已建立的连接有可读事件发生了，调用用户传入的onMessage操作
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0) //断开
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if (channel_->isWriting())
    {   
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if (n > 0)
        {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this())
                    );
                }
                if (state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else {
            LOG_ERROR("TcpConnection::handleWrite\n");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection::handleWrite  fd=%d id down no more writing \n", channel_->fd());
    }
}
//poller =>channel::closeCallback=>TcpConnection::handleClose
void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d, state=%d\n", channel_->fd(), (int)state_);

    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr guardThis(shared_from_this());

    //执行连接关闭的回调
    connectionCallback_(guardThis);
    //关闭连接的回调
    closeCallback_(guardThis); //执行TcpServer::removeConnection
}
void TcpConnection::handleError()
{
    int optval;
    int err = 0;
    socklen_t optlen = static_cast<socklen_t>(sizeof optval);
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }

    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}


