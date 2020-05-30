#pragma once
#include "buf_io.h"
#include "buf_pool.h"
#include <assert.h>
#include <unistd.h>
#include <stdint.h>

/*
 * 给业务层提供的最后tcp_buffer结构
   法1，每次数据包都有固定长度，不能以缓冲区为准，以防止数据存放错误
   法2，将较长数据包切成小包，让网络模块自己拼接，并实现一个小顶堆，最早数据且过期失效数据进行回复
   注：其实保证两端发送规则一样就可以了
*/
class buf_reactor 
{
public:
    buf_reactor();
    ~buf_reactor();
    const int length() const;
    void pop(int len);
    void clear();
protected:
    buf_io *_buf;
};

//读(输入) 缓存buffer
class buf_rcv : public buf_reactor
{
public:
    int read_data(int fd);          //从一个fd中读取数据到reactor_buf中
    const char *data() const;       //取出读到的数据
    void adjust();                  //重置缓冲区
};

//写(输出)  缓存buffer
class buf_snd : public buf_reactor 
{
public:  
    int send_data(const char *data, int datalen);   //将一段数据 写到一个reactor_buf中
    int write2peer(int fd);                           //将reactor_buf中的数据写到一个fd中
};
