#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#include "net_tcp_server.h"
#include "net_tcp__conn.h"
#include "buf_reactor.h"
#include "config_file.h"

// ==== 链接资源管理   ====
tcp_conn ** tcp_server::conns = NULL;										//全部已经在线的连接信息
						
int tcp_server::_max_conns 	= 0;      										//最多链接个数;
int tcp_server::_cur_conns 	= 0;											//当前链接刻度

pthread_mutex_t tcp_server::_conns_mutex = PTHREAD_MUTEX_INITIALIZER;		//保护_cur_conns刻度修改的锁

conn_callback 	tcp_server::conn_start_cb = NULL;							//创建链接之后的回调函数
void*           tcp_server::conn_start_cb_args = NULL;
conn_callback 	tcp_server::conn_close_cb = NULL;							//销毁链接之前的回调函数
void*           tcp_server::conn_close_cb_args = NULL;

// ==== 消息分发路由   ===
msg_router tcp_server::router;				

void tcp_server::incr_conn(int connfd, tcp_conn *conn)			//新增一个新建的连接
{
    pthread_mutex_lock(&_conns_mutex);
    conns[connfd] = conn;
    _cur_conns++;
    pthread_mutex_unlock(&_conns_mutex);
}
void tcp_server::decr_conn(int connfd)							//减少一个断开的连接
{
    pthread_mutex_lock(&_conns_mutex);
    conns[connfd] = NULL;
    _cur_conns--;
    pthread_mutex_unlock(&_conns_mutex);
}
void tcp_server::get_conn_num(int *curr_conn)						//得到当前链接的刻度
{
    pthread_mutex_lock(&_conns_mutex);
    *curr_conn = _cur_conns;
    pthread_mutex_unlock(&_conns_mutex);
}

// =======================

//listen fd 客户端有新链接请求过来的回调函数
void accept_callback(event_loop *loop, int fd, void *args)
{
    tcp_server *server = (tcp_server*)args;
    server->do_accept();
}

void tcp_server::set_conn_start(conn_callback cb, void *args)
{
    conn_start_cb = cb;
    conn_start_cb_args = args;
}
void tcp_server::set_conn_close(conn_callback cb, void *args)	
{
    conn_close_cb = cb;
    conn_close_cb_args = args;
}

//server的构造函数
tcp_server::tcp_server(event_loop *loop, const char *ip, uint16_t port)
{
    bzero(&_connaddr, sizeof(_connaddr));
    
    //忽略 SIGHUP, SIGPIPE
    if (signal(SIGHUP, SIG_IGN) == SIG_ERR) {			 	//  SIGPIPE :如果客户端关闭，服务端再次write就会产生
        fprintf(stderr, "signal ignore SIGHUP\n");				
    }
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {				//  SIGHUP	:如果terminal关闭，会给当前进程发送该信号
        fprintf(stderr, "signal ignore SIGPIPE\n");
    }

    _sockfd = socket(AF_INET, SOCK_STREAM /*| SOCK_NONBLOCK*/ | SOCK_CLOEXEC, IPPROTO_TCP);
    if (_sockfd == -1) {
        fprintf(stderr, "tcp_server::socket()\n");
        exit(1);
    }

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_aton(ip, &server_addr.sin_addr);
    server_addr.sin_port = htons(port);
    _addrlen = sizeof(server_addr);

    int op = 1;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op)) < 0) {
        fprintf(stderr, "setsocketopt SO_REUSEADDR\n");
    }

    if (bind(_sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "bind error\n");
        exit(1);
    }

    if (listen(_sockfd, 500) == -1) {
        fprintf(stderr, "listen error\n");
        exit(1);
    }

    _max_conns = config_file::instance()->GetNumber("reactor", "maxConn", 1024);		// 创建链接管理数组
    conns = new tcp_conn*[_max_conns+3];    											//因stdin,stdout,stderr已被占用，再新开fd一定是从3开始,所以不加3就会栈溢出
    if (conns == NULL) {
        fprintf(stderr, "new conns[%d] error\n", _max_conns);
        exit(1);
    }
    bzero(conns, sizeof(tcp_conn*)*(_max_conns+3));

    int thread_cnt = config_file::instance()->GetNumber("reactor", "threadNum", 10);	// 创建线程池
    if (thread_cnt > 0) {
        _thread_pool = new thread_pool(thread_cnt);
        if (_thread_pool == NULL) {
            fprintf(stderr, "tcp_server new thread_pool error\n");
            exit(1);
        }
    }
    
    _loop = loop;
    _loop->add_io_event(_sockfd, EPOLLIN, accept_callback, this);        // 注册_socket读事件-->accept处理
}

//开始提供创建链接服务
void tcp_server::do_accept()
{
    int connfd;    
    while(true) 
	{
        //printf("begin accept\n");
        connfd = accept(_sockfd, (struct sockaddr*)&_connaddr, &_addrlen);
        if (connfd == -1) {                             //accept与客户端创建链接
            if (errno == EINTR) {
                fprintf(stderr, "accept errno=EINTR\n");
                continue;
            } else if (errno == EMFILE) {		                //建立链接过多，资源不够
                fprintf(stderr, "accept errno=EMFILE\n");
            } else if (errno == EAGAIN) {
                fprintf(stderr, "accept errno=EAGAIN\n");
                break;
            } else {
                fprintf(stderr, "accept error  %s\n", strerror(errno));   //perror
                exit(1);
            }
        } else {                                         //accept succ!
            int cur_conns;
            get_conn_num(&cur_conns);
            if (cur_conns >= _max_conns) {			            //1 判断链接数量
                fprintf(stderr, "too many connections, max = %d\n", _max_conns);
                close(connfd);
            } else {
                if (_thread_pool != NULL) {		                    //启动多线程模式 创建链接
                    task_queue<task_msg> *queue = _thread_pool->get_thread();	//1 选择一个线程来处理
                    task_msg        task;				                        //2 创建一个新建链接的消息任务
                    task.type       = task_msg::NEW_CONN;
                    task.connfd     = connfd;
                    queue->send(task);			                                //3 添加到消息队列中，让对应的thread进程event_loop处理
                } else {                                            //启动单线程模式
                    tcp_conn *conn = new tcp_conn(connfd, _loop);
                    if (conn == NULL) {
                        fprintf(stderr, "new tcp_conn error\n");
                        exit(1);
                    }
                    printf("[tcp_server]: get new connection succ!\n");
                    break;
                }
            }
        }
    }
}

//链接对象释放的析构
tcp_server::~tcp_server()
{
    _loop->del_io_event(_sockfd);           //将全部的事件删除
    close(_sockfd);                         //关闭套接字
}
