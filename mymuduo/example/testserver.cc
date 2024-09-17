#include<mymuduo/TcpServer.h>
#include<mymuduo/Logger.h>
#include <string>
#include <functional>
class EchoServer
{
    public:
    EchoServer(EventLoop *Loop, const InetAddress &addr, const std::string name)
        :server_(Loop, addr, name),
        loop_(Loop)
    {
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1)
        );

        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
        );
        server_.setThreadNum(3);
    }

    void start()
    {
        server_.start();
    }

    private:

    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            LOG_INFO("conn UP : %s", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("conn down:%s ", conn->peerAddress().toIpPort().c_str());
        }
    }
    //可读写事件回调
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown();
    }
    EventLoop *loop_;
    TcpServer server_;
};

int main()
{
    EventLoop loop;
    InetAddress addr(8000);
    EchoServer server(&loop, addr, "Echoserver"); //Acceptor non-blocking listenfd create bind
    server.start(); //listen loopthread listenfd => acceptchannel => mailoop =>

    loop.loop();  //启动mainloop的底层poller
    return 0;
}