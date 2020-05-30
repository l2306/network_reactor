#pragma once

#include <stdio.h>

/*
 * 网络通信的抽象类，模块收发消息需要继承
 */

class net_connection
{
public:
    net_connection():param(NULL) {}
    virtual int snd_msg(const char *data, int len, int msgid) = 0;     //发送消息的接口
    virtual int get_fd() = 0;
    void *param;                                                        //TCP客户端自定义的参数
};

    //回调函数类型   创建链接/销毁链接时用
typedef void (*conn_callback)(net_connection *conn, void *args);
