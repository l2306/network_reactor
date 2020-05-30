#include "buf_reactor.h"
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

buf_reactor::buf_reactor() 
{
    _buf = NULL;
}

buf_reactor::~buf_reactor()
{
    clear();
}

const int buf_reactor::length() const 
{
    return _buf != NULL? _buf->len : 0;
}

void buf_reactor::pop(int len) 
{
    assert(_buf != NULL && len <= _buf->len);
    _buf->pop(len);

    if(_buf->len == 0) {						//当此时_buf的可用长度已经为0
        buf_pool::instance()->revert(_buf);		//将_buf重新放回buf_pool中
        _buf = NULL;
    }
}

void buf_reactor::clear()
{
    if (_buf != NULL)  {
        buf_pool::instance()->revert(_buf);		 //将_buf重新放回buf_pool中
        _buf = NULL;
    }
}

//================= buf_reactor ===============

//从一个fd中读取数据到buf_reactor中
int buf_rcv::read_data(int fd)
{
    int need_read;//硬件有多少数据可以读  为了一次性读出所有的数据

    if (ioctl(fd, FIONREAD, &need_read) == -1) {			//需要给fd设置FIONREAD 得到read缓冲中有多少数据是可以读取的
        fprintf(stderr, "ioctl FIONREAD\n");
        return -1;
    }
    
    if (_buf == NULL) {
        _buf = buf_pool::instance()->alloc_buf(need_read);		//如果buf_io为空,从内存池申请
        if (_buf == NULL) {
            fprintf(stderr, "no idle buf for alloc\n");
            return -1;
        }
    } else {
        assert(_buf->head == 0);				//如果buf_io可用，判断是否够存
        if (_buf->capacity - _buf->len < (int)need_read) {
            buf_io *new_buf = buf_pool::instance()->alloc_buf(need_read+_buf->len);	//不够存，冲内存池申请
            if (new_buf == NULL) {
                fprintf(stderr, "no ilde buf for alloc\n");
                fprintf(stderr, "_buf->capacity - _buf->len < (int)need_read %d %d %d", _buf->capacity, _buf->len, need_read);
                return -1;
            }           
            new_buf->copy(_buf);					//将之前的_buf的数据考到新申请的buf中
            buf_pool::instance()->revert(_buf);		//将之前的_buf放回内存池中
            _buf = new_buf;							//新申请的buf成为当前buf_io
        }
    }

    //读取数据
    int already_read = 0;
    do { 								//读取的数据拼接到之前的数据之后
        if(need_read == 0)			//可能是read阻塞读数据的模式，对方未写数据
            already_read = read(fd, _buf->data + _buf->len, m4K);
        else
            already_read = read(fd, _buf->data + _buf->len, need_read);
    } while (already_read == -1 && errno == EINTR); //systemCall引起的中断 继续读取
    if (already_read > 0)  {
        if (need_read != 0)
            assert(already_read == need_read);
        _buf->len += already_read;
    }

    return already_read;
}

//取出读到的数据
const char *buf_rcv::data() const 
{
    return _buf != NULL ? _buf->data + _buf->head : NULL;
}

//重置缓冲区
void buf_rcv::adjust()
{
    if (_buf != NULL) 
        _buf->adjust();
}

//================= buf_rcv ===========

//将一段数据 写到一个buf_reactor中
int buf_snd::send_data(const char *data, int datalen)
{
    if (_buf == NULL) {
        _buf = buf_pool::instance()->alloc_buf(datalen);		//如果buf_io为空,从内存池申请
        if (_buf == NULL) {
            fprintf(stderr, "no idle buf for alloc\n");
            return -1;
        }
    } else {
        assert(_buf->head == 0);	
        if (_buf->capacity - _buf->len < datalen)   		//如果buf_io可用，判断是否够存
        {    
            buf_io *new_buf = buf_pool::instance()->alloc_buf(datalen+_buf->len);		//不够存，冲内存池申请
            if (new_buf == NULL) {
                fprintf(stderr, "no ilde buf for alloc\n");
                return -1;
            }
            new_buf->copy(_buf);				 //拷出已有数据
            buf_pool::instance()->revert(_buf);	 //放回到内存池
            _buf = new_buf;					     //buf另行赋值
        }
    }

    memcpy(_buf->data + _buf->len, data, datalen);		 //将data数据拷贝到buf_io中,拼接到后面
    _buf->len += datalen;

    return 0;
}

//将buf_reactor中的数据写到一个fd中
int buf_snd::write2peer(int fd)
{
    assert(_buf != NULL && _buf->head == 0);
    int already_write = 0;
    do { 
        already_write = write(fd, _buf->data, _buf->len);
    } while (already_write == -1 && errno == EINTR); 		//systemCall引起的中断，继续写

    if (already_write > 0) {
        _buf->pop(already_write);							//已经处理的数据清空
        _buf->adjust();		 								//未处理数据前置，覆盖老数据
    }
    if (already_write == -1 && errno == EAGAIN)             //若fd非阻塞，可能EAGAIN错误
        already_write = 0;									//不是错误，返回0表示目前是不可以继续写的

    return already_write;
}


