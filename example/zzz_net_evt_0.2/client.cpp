#include "net_tcp_client.h"
#include <stdio.h>
#include <string.h>

void cb_rsp_fun(const char *data, uint32_t len, int msgid, net_connection  *conn, void *user_data)
{
    printf(" rcv S: id:[%8x] len:[%d] data:[--]\n", msgid, len);
}

void cb_rsp_prt(const char *data, uint32_t len, int msgid, net_connection  *conn, void *user_data)
{
    printf(" rcv S: id:[%8x] len:[%d] data:[%s]\n", msgid, len, data);
}

//客户端业务
void cb_rsp_fun11(const char *data, uint32_t len, int msgid, net_connection  *conn, void *user_data)
{
    char *str = NULL;
    str = (char*)malloc(len+1);
    memset(str, 0, len+1);
    memcpy(str, data, len);
    printf(" rcv S: id:[%8x] len:[%d] data:[%s]\n", msgid, len, str);
    printf("        conn para = %s\n", (const char *)conn->param);
}

//客户端销毁的回调
void on_client_build(net_connection *conn, void *args)
{
    printf(" %s\n", __FUNCTION__);
    int msgid = 01 ; 
    const char *msg = "C say Hi !";
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
    tcp_client client(&loop, "127.0.0.1", 9999, "client_v0.6");  //创建tcp客户端

    //注册消息路由业务
    client.add_msg_router(00, cb_rsp_fun);
    client.add_msg_router(01, cb_rsp_prt);
    client.add_msg_router(21, cb_rsp_fun11);

    //设置hook函数
    client.set_conn_start(on_client_build);
    client.set_conn_close(on_client_break);

    loop.event_process();                                       //开启事件监听

    return 0;
}
