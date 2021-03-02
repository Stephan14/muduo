// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include "muduo/net/Buffer.h"

#include "muduo/net/SocketsOps.h"

#include <errno.h>
#include <sys/uio.h>

using namespace muduo;
using namespace muduo::net;

const char Buffer::kCRLF[] = "\r\n";

const size_t Buffer::kCheapPrepend;
const size_t Buffer::kInitialSize;

ssize_t Buffer::readFd(int fd, int* savedErrno)
{
  // saved an ioctl()/FIONREAD call to tell how much to read
  char extrabuf[65536];//缓冲区取自栈
  struct iovec vec[2];//使用向量io，将单个流读到多个缓冲区
  const size_t writable = writableBytes();
  vec[0].iov_base = begin()+writerIndex_;
  vec[0].iov_len = writable;
  vec[1].iov_base = extrabuf;
  vec[1].iov_len = sizeof extrabuf;
  // when there is enough space in this buffer, don't read into extrabuf.
  // when extrabuf is used, we read 128k-1 bytes at most.
  const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
  const ssize_t n = sockets::readv(fd, vec, iovcnt);//只调用一次read函数，没有反复调用，采用水平触发，这样不会丢失消息；对于低延迟消息来说减少系统调用并保证多个连接的公平性。如果是边缘触发，至少调用两次read
  if (n < 0)
  {
    *savedErrno = errno;
  }
  else if (implicit_cast<size_t>(n) <= writable)
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

  /* 1. 当您执行大量随机(即非顺序)读/写操作时，您想使用Scatter/Gather IO，
   * 并且您希望节省上下文切换/系统调用-从这种意义上讲，Scatter/Gather是一种批处理形式。
   * 但是，除非您有非常快的磁盘(或更可能有大量磁盘)，否则系统调用成本可以忽略不计
   * 2. Scatter/gather在有些场景下会非常有用，比如需要处理多份分开传输的数据。
   * 举例来说，假设一个消息包含了header和body，我们可能会把header和body保存在
   * 不同独立buffer中，这种分开处理header与body的做法会使开发更简明
   * */
  return n;
}

