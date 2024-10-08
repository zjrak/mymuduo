#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <unistd.h>
/* 
//从fd上读取数据 Poller工作在LT模式

BUffer缓冲区是有大小的，但是fd上读取数据的时候，却不知道tcp数据最终的大小
 */
ssize_t Buffer::readFd(int fd, int* savedErrno)
{
    //栈上的内存空间
    char extrabuf[65536] = {0};
    struct iovec vec[2];
    //底层缓冲区剩余的可写空间大小
    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0)
    {
        *savedErrno = errno;
    }
    else if (n <= writable)  //buffer的可写缓冲区已经够存数据
    {
        writerIndex_ += n;
    }
    else
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    // if (n == writable + sizeof extrabuf)
    // {
    //   goto line_30;
    // }
    return n;
}

ssize_t Buffer::writeFd(int fd, int* savedErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *savedErrno = errno;
    }
    return n;
}