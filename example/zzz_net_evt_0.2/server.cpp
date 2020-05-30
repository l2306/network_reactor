#include "net_tcp_server.h"
#include <string>
#include <string.h>
#include "config_file.h"

//打印信息回调函数
void cb_req_fun(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data)
{
    printf(" rcv C: id:[%8x] len:[%d] data:[--]  \n", msgid, len);
}

void cb_req_prt(const char *data, uint32_t len, int msgid, net_connection  *conn, void *user_data)
{
    printf(" rcv C: id:[%8x] len:[%d] data:[%s]\n", msgid, len, data);
}

//回显业务的回调函数
void cb_req_fun1(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data)
{
    printf("%s \n", __FUNCTION__);
    printf(" rcv C: id:[%8x] len:[%d] data:[%s]  \n", msgid, len, data);
    conn->snd_msg(data, len, msgid);                         //直接回显
}

//新客户端创建的回调
void on_client_build(net_connection *conn, void *args)
{
    int msgid = 0x01;
    const char *msg = "welcome! you online.";
    conn->snd_msg(msg, strlen(msg), msgid);
    {
        int msgid = 0x01;
        const char *msg = "a pie of cook";
        conn->snd_msg(msg, strlen(msg), msgid);
    }

    const char *conn_param_test = "I am the conn for you!";
    conn->param = (void*)conn_param_test;           //向net_connection绑定一个参数
}

//客户端销毁的回调
void on_client_break(net_connection *conn, void *args)
{
    printf(" %s\n", __FUNCTION__);
}

int main() 
{
    event_loop loop;
    
    config_file::setPath("./serv.conf");              //加载配置文件
    std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
    short port = config_file::instance()->GetNumber("reactor", "port", 8888);
    printf("ip = %s, port = %d\n", ip.c_str(), port);

    tcp_server server(&loop, ip.c_str(), port);

    server.add_msg_router(00, cb_req_fun);         //注册消息业务路由
    server.add_msg_router(01, cb_req_prt);         //注册消息业务路由
    server.add_msg_router(21, cb_req_fun1);

    server.set_conn_start(on_client_build);          //注册链接hook回调
    server.set_conn_close(on_client_break);

    loop.event_process();

    return 0;
}
