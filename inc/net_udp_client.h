#pragma once

#include "net_connection.h"
#include "message.h"
#include "event_loop.h"

class udp_client: public net_connection
{
public:
    udp_client(event_loop *loop, const char *ip, uint16_t port);
    ~udp_client();
    void add_msg_router(int msgid, msg_callback *cb, void *user_data = NULL);
    virtual int snd_msg(const char *data, int msglen, int msgid);
    int get_fd();
    void do_read();
    
private:
    int         _sockfd;
    char        _buf_rd[MSG_LEN_MAX];
    char        _buf_wt[MSG_LEN_MAX];
    event_loop  *_loop;                             //事件触发
    msg_router  _router;                            //消息路由分发
};
