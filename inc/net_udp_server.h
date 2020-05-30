#pragma  once

#include <netinet/in.h>
#include "event_loop.h"
#include "net_connection.h"
#include "message.h"

class udp_server :public net_connection 
{
public:
    udp_server(event_loop *loop, const char *ip, uint16_t port);
    ~udp_server();
    virtual int snd_msg(const char *data, int msglen, int msgid);
    void add_msg_router(int msgid, msg_callback* cb, void *user_data = NULL);       //注册消息路由回调函数
    int get_fd();                                       							//得到fd	
    void do_read();                                     							//处理消息业务
						
private:					
    int                 _sockfd;					
    char                _buf_rd[MSG_LEN_MAX];					
    char                _buf_wt[MSG_LEN_MAX];					
    event_loop*         _loop;                              						//事件触发
    struct sockaddr_in  _client_addr;                       						//服务端ip
    socklen_t           _client_addrlen;						
    msg_router          _router;                            						//消息路由分发
};
