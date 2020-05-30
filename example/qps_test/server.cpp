#include <string>
#include <string.h>
#include <iostream>
#include "config_file.h"
#include "net_tcp_server.h"
#include "msg_echo.pb.h"


//回显业务的回调函数
void callback_busi(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data)
{
    qps_test::Msg_echo  request, response;  

    request.ParseFromArray(data, len); 
    response.set_id(request.id());
    response.set_content(request.content()+" @server signed@");
    std::string strRsp;
    response.SerializeToString(&strRsp);

	std::cout<<"  rcv C"<< request.content()<<std::endl;
		
    conn->snd_msg(strRsp.c_str(), strRsp.size(), msgid);
}


int main() 
{
    event_loop loop;
    
    config_file::setPath("./serv.conf");                //加载配置文件
    std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
    short port = config_file::instance()->GetNumber("reactor", "port", 8888);

    printf("ip = %s, port = %d\n", ip.c_str(), port);

    tcp_server server(&loop, ip.c_str(), port);

    //注册消息业务路由
    server.add_msg_router(1, callback_busi);

    loop.event_process();

    return 0;
}
