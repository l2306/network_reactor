#include "net_tcp_server.h"
#include <string>
#include <string.h>
#include "config_file.h"

tcp_server *server;

void print_lars_task(event_loop *loop, void *args)
{
    printf("======= Active Task Func! ========\n");
    listen_fd_set fds;
    loop->get_listen_fds(fds);                              //不同线程的loop，返回的fds是不同的
    
    listen_fd_set::iterator it;                             //可向所有fds触发
    for (it = fds.begin(); it != fds.end(); it++) {         //遍历fds
        int fd = *it;
        tcp_conn *conn = tcp_server::conns[fd];             //取出fd
        if (conn != NULL) {
            int msgid = 101;
            const char *msg = "Hello I am a Task!";
            conn->snd_msg(msg, strlen(msg), msgid);
        }
    }
}

//回显业务的回调函数
void callback_busi(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data)
{
    printf("callback_busi ...\n");
    conn->snd_msg(data, len, msgid);       //直接回显
}

//打印信息回调函数
void print_busi(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data)
{
    printf(" recv client: id:[%d] len:[%d] data:[%s]\n", msgid, len, data);
}


//新客户端创建的回调
void on_client_build(net_connection *conn, void *args)
{
    int msgid = 101;
    const char *msg = "welcome! you online..";
    conn->snd_msg(msg, strlen(msg), msgid);

//    server->thread_poll()->send_task(print_lars_task);      //创建链接成功之后触发任务
}

//客户端销毁的回调
void on_client_break(net_connection *conn, void *args)
{
    printf(" %s\n", __FUNCTION__);
}


int main() 
{
    event_loop loop;

    config_file::setPath("./serv.conf");
    std::string     ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
    short           port = config_file::instance()->GetNumber("reactor", "port", 8888);
    printf("ip = %s, port = %d\n", ip.c_str(), port);
    server = new tcp_server(&loop, ip.c_str(), port);

    server->add_msg_router(1, callback_busi);               //注册消息业务路由
    server->add_msg_router(2, print_busi);

    server->set_conn_start(on_client_build);                //注册链接hook回调
    server->set_conn_close(on_client_break);

    loop.event_process();
    return 0;
}
