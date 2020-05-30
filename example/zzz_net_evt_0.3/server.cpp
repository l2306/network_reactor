#include <string>
#include <string.h>
#include "config_file.h"
#include "net_udp_server.h"


//回显业务的回调函数
void callback_busi(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data)
{
    printf("callback_busi ...\n");
    conn->snd_msg(data, len, msgid);            //直接回显
}


int main() 
{
    event_loop loop;
   
    config_file::setPath("./serv.conf");                //加载配置文件
    std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
    short port = config_file::instance()->GetNumber("reactor", "port", 8888);
    printf("ip = %s, port = %d\n", ip.c_str(), port);

    udp_server server(&loop, ip.c_str(), port);
    
    server.add_msg_router(1, callback_busi);            //注册消息业务路由

    loop.event_process();

    return 0;
}
