#include "net_tcp_client.h"
#include <stdio.h>
#include <string.h>


//客户端业务
void busi(const char *data, uint32_t len, int msgid, net_connection  *conn, void *user_data)
{
    char *str = NULL;
    str = (char*)malloc(len+1);
    memset(str, 0, len+1);
    memcpy(str, data, len);
    printf(" rcv S: id:[%d] len:[%d] data:[%s]\n", msgid, len, str);
}


//客户端销毁的回调
void on_client_build(net_connection *conn, void *args)
{
    int msgid = 1; 
    const char *msg = "Hello Lars!";
    conn->snd_msg(msg, strlen(msg), msgid);
}

//客户端销毁的回调
void on_client_break(net_connection *conn, void *args) 
{
    printf(" %s\n", __FUNCTION__);
}

int main() 
{
    event_loop loop;

    tcp_client client(&loop, "127.0.0.1", 7777, "clientv0.6");      //创建tcp客户端

    client.add_msg_router(1, busi);                                 //注册消息路由业务
    client.add_msg_router(101, busi);

    client.set_conn_start(on_client_build);                         //设置hook函数
    client.set_conn_close(on_client_break);

    loop.event_process();                                           //开启事件监听

    return 0;
}



