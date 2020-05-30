#pragma once

#include <netinet/in.h>
#include "event_loop.h"
#include "net_tcp__conn.h"
#include "message.h"
#include "thread_pool.h"

class tcp_server
{ 
public: 
    tcp_server(event_loop* loop, const char *ip, uint16_t port); 
    void do_accept();			  									//开始提供创建链接服务
    ~tcp_server();	
    void add_msg_router(int msgid, msg_callback *cb, void *user_data = NULL) {		 //注册消息路由回调函数
        router.reg_msg_router(msgid, cb, user_data);
    }
    thread_pool *thread_poll() { return _thread_pool; }				//获取当前server的线程池
private: 
    int 				_sockfd; 					//套接字
    event_loop          *_loop;                     //event_loop事件机制
    socklen_t           _addrlen;                   //客户端地址长度
    struct sockaddr_in  _connaddr;                  //客户端链接地址							
public:
    static msg_router router;                       //---- 消息分发路由 ----		
public:                                             //---- 客户端链接管理部分-----
    static void incr_conn(int connfd, tcp_conn *conn);    	    //新增一个新建的连接
    static void decr_conn(int connfd);    					    //减少一个断开的连接
    static void get_conn_num(int *curr_conn);     				//得到当前链接的刻度
    static tcp_conn **conns;        							//全部已经在线的连接信息

    // ------- 创建链接/销毁链接 Hook 部分 -----
    static void set_conn_start(conn_callback cb, void *args = NULL);	//设置链接的创建hook函数
    static void set_conn_close(conn_callback cb, void *args = NULL);	//设置链接的销毁hook函数
    static conn_callback    conn_start_cb;		  	//创建链接之后要触发的 回调函数
    static void             *conn_start_cb_args;
    static conn_callback    conn_close_cb;			//销毁链接之前要触发的 回调函数
    static void             *conn_close_cb_args;

private:
    static int 					_max_conns;         //最大接客个数
    static int 					_cur_conns;         //当前链接刻度
    static pthread_mutex_t		_conns_mutex; 		//保护数量变化
    thread_pool 				*_thread_pool;		//线程池
}; 
