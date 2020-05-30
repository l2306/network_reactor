#pragma once

#include <stdint.h>
#include <ext/hash_map>
#include "net_connection.h"

//解决tcp粘包问题的消息头
struct msg_head {
    int msgid;
    int msglen;
};

#define MSG_HEAD_LEN 8                          //消息头的二进制长度，固定数
#define MSG_LEN_MAX (65535 - MSG_HEAD_LEN)      //消息头+消息体的最大长度限制

//msg 业务回调函数原型
typedef void msg_callback(const char *data, uint32_t len, int msgid, net_connection *net_conn, void *user_data);

//消息路由分发机制
class msg_router 
{
public:
    msg_router():_router(),_args() {
        //printf("msg_router init...\n");
        printf(" \n");
    }  

    //给一个消息ID注册一个对应的回调业务函数
    int reg_msg_router(int msgid, msg_callback *msg_cb, void *user_data) 
    {
        if(_router.find(msgid) != _router.end())
            return -1;                                  //该msgID的回调业务已经存在
        printf("add msg cb msgid = %d\n", msgid);
        _router[msgid]  = msg_cb;
        _args[msgid]    = user_data;
        return 0;
    }

    //调用注册的对应的回调业务函数
    void call(int msgid, uint32_t msglen, const char *data, net_connection *net_conn) 
    {
        //printf(" call msgid = %d\n call data = %s\n call msglen = %d", msgid, data, msglen);
        if (_router.find(msgid) == _router.end()) {         //判断msgid对应的回调是否存在
            fprintf(stderr, "msgid %d is not register!\n", msgid);
            return;
        }
        msg_callback    *callback   = _router[msgid];        //直接取出回调函数，执行
        void            *user_data  = _args[msgid];
        callback(data, msglen, msgid, net_conn, user_data);
    }

private:
    __gnu_cxx::hash_map<int, msg_callback*> _router;         //消息路由分发
    __gnu_cxx::hash_map<int, void*>         _args;           //回调函数参数
};
