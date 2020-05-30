#pragma once

#include "buf_reactor.h"
#include "event_loop.h"
#include "net_connection.h"

//一个tcp的连接信息
class tcp_conn : public net_connection
{
public:
    tcp_conn(int connfd, event_loop *loop);						//初始化
    void do_read();				    							//处理读
    void do_write();				  							//处理写
    void clean_conn();				  							//销毁连接
    int snd_msg(const char *data, int msglen, int msgid);	    //发送消息
    int get_fd();												//得到fd
private:
    int 			_connfd;									//当前链接fd
    event_loop 		*_loop;							   			//该连接归属的event_poll
    buf_snd 		_obuf;     							 		//输出buf
    buf_rcv 		_ibuf;									  	//输入buf
};
