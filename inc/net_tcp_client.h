#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "buf_io.h"
#include "event_loop.h"
#include "message.h"
#include "net_connection.h"


class tcp_client : public net_connection
{
public:
    tcp_client(event_loop *loop, const char *ip, unsigned short port,  const char *name);    //初始化客户端套接字
    int snd_msg(const char *data, int msglen, int msgid);          //发送message方法
	void do_connect();  //创建链接
    int get_fd();        //得到fd
    int do_read();        //处理读业务
    int do_write();      //处理写业务
    void clean_conn();          //释放链接资源
    ~tcp_client();

    void set_msg_callback(msg_callback *msg_cb) {  _msg_callback = msg_cb;}	    //xx 设置业务处理回调函数
    
    void add_msg_router(int msgid, msg_callback *cb, void *user_data = NULL)    //注册消息路由回调函数
         { _router.reg_msg_router(msgid, cb, user_data);  }
    

    //----- 链接创建/销毁回调Hook ----
    void set_conn_start(conn_callback cb, void *args = NULL)  		//设置链接的创建hook函数
    {
        _conn_start_cb = cb;
        _conn_start_cb_args = args;
    }
    void set_conn_close(conn_callback cb, void *args = NULL) {		//设置链接的销毁hook函数
        _conn_close_cb = cb;
        _conn_close_cb_args = args;
    }
    
    
    conn_callback       _conn_start_cb;         //创建链接之后要触发的 回调函数
    void                *_conn_start_cb_args;
    conn_callback       _conn_close_cb;         //销毁链接之前要触发的 回调函数
    void                *_conn_close_cb_args;
    bool                connected;              //链接是否创建成功
    struct sockaddr_in  _server_addr;           //server端地址
    buf_io              _obuf;
    buf_io              _ibuf;

private:
    int             _sockfd;
    socklen_t       _addrlen;
    msg_router      _router;    		//处理消息的分发路由
    msg_callback 	*_msg_callback; 	//xx 单路由模式去掉
    event_loop      *_loop;          	//客户端的事件处理机制
    const char      *_name;          	//当前客户端的名称 用户记录日志
};
