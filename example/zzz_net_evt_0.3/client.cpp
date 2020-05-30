#include <stdio.h>
#include <string.h>
#include "net_udp_client.h"

//客户端业务
void busi(const char *data, uint32_t len, int msgid, net_connection  *conn, void *user_data)
{
    char *str = NULL;
    str = (char*)malloc(len+1);
    memset(str, 0, len+1);
    memcpy(str, data, len);
    printf(" recv server: id:[%d] len:[%d] data:[%s]\n", msgid, len, str);
}

int main() 
{
    event_loop loop;

    udp_client client(&loop, "127.0.0.1", 7777);        //创建udp客户端

    client.add_msg_router(1, busi);                     //注册消息路由业务

    int msgid = 1;                                      //发消息
    const char *msg = "C say Hi !";
    client.snd_msg(msg, strlen(msg), msgid);

    loop.event_process();                               //开启事件监听

    return 0;
}



