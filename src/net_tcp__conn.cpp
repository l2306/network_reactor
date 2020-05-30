#include "net_tcp_server.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>

#include "message.h"
#include "buf_reactor.h"
#include "net_tcp__conn.h"

//回显业务
void callback_busi(const char *data, uint32_t len, int msgid, void *args, tcp_conn *conn)
{
    conn->snd_msg(data, len, msgid);
}

//连接的读事件回调 
static void callback_conn_rd(event_loop *loop, int fd, void *args)
{
    ((tcp_conn*)args)->do_read();
}
//连接的写事件回调
static void callback_conn_wt(event_loop *loop, int fd, void *args)
{
    tcp_conn *conn = (tcp_conn*)args;
    if(NULL != conn)
        conn->do_write();
}

//初始化tcp_conn
tcp_conn::tcp_conn(int connfd, event_loop *loop)
{
    _connfd = connfd;
    _loop = loop;
    
    int flag = fcntl(_connfd, F_GETFL, 0);
    fcntl(_connfd, F_SETFL, O_NONBLOCK|flag);		//设置connfd非阻塞     ？？一旦写失败立即执行其他操作

    int op = 1;                                                             // TCP_NODELAY禁止读写缓存，降低小包延迟
    setsockopt(_connfd, IPPROTO_TCP, TCP_NODELAY, &op, sizeof(op));			// netinet/in.h netinet/tcp.h

    if (tcp_server::conn_start_cb)	
        tcp_server::conn_start_cb(this, tcp_server::conn_start_cb_args);	// 若注册了链接建立回则调用

    _loop->add_io_event(_connfd, EPOLLIN, callback_conn_rd, this);			//添加读事件到event_loop 

    tcp_server::incr_conn(_connfd, this);      							//添加该链到对应的tcp_server
}

int tcp_conn::get_fd()
{
    return this->_connfd;
}

//处理读业务
void tcp_conn::do_read()
{
    int ret = _ibuf.read_data(_connfd);				//从套接字读取数据
    if (ret == -1) {
        fprintf(stderr, "read data from socket\n");
        this->clean_conn();
        return ;
    } else if ( ret == 0) {							//对端正常关闭
        printf("connection closed by peer\n");
        clean_conn();
        return ;
    }
   
    msg_head head;    								 //解析msg_head数据    
    while (_ibuf.length() >= MSG_HEAD_LEN)            // 用while，可能一次性读取多个完整包过来]
    {  
        memcpy(&head, _ibuf.data(), MSG_HEAD_LEN);			  //2.1 读取msg_head头部，固定长度MSG_HEAD_LEN    
        if(head.msglen > MSG_LEN_MAX || head.msglen < 0) {
            fprintf(stderr, "data format error, need close, msglen = %d\n", head.msglen);
            this->clean_conn();
            break;
        }
        if (_ibuf.length() < MSG_HEAD_LEN + head.msglen) {		 //缓存buf中剩余的数据，小于实际上应该接受的数据  //说明是一个不完整的包，应该抛弃
            break;
        }
        _ibuf.pop(MSG_HEAD_LEN);										 	        //后移
        tcp_server::router.call(head.msgid, head.msglen, _ibuf.data(), this);	//处理ibuf.data()业务数据 //消息包路由模式
        _ibuf.pop(head.msglen);			 										//消息体处理完了,往后便宜msglen长度
    }
    _ibuf.adjust();
    return ;
}

//处理写业务
void tcp_conn::do_write()
{
    //do_write是触发玩event事件要处理的事情，
    //  应该是直接将out_buf里的数据io写会对方客户端 
    //  而不是在这里组装一个message再发
    //  组装message的过程应该是主动调用
    
    while (_obuf.length())                    //只要obuf中有数据就写   
    {
        int ret = _obuf.write2peer(_connfd);
        if (ret == 0)
            break;                          //不是错误，仅返回0表示不可继续写
        if (ret == -1) {
            fprintf(stderr, "write2peer error, close conn!\n");
            this->clean_conn();
            return ;
        }        
    }
    if (_obuf.length() == 0) {
        _loop->del_io_event(_connfd, EPOLLOUT);     //数据已经全部写完，将_connfd的写事件取消掉
    }
    return ;
}

//发送消息的方法
int tcp_conn::snd_msg(const char *data, int msglen, int msgid)
{
    bool active_epollout = false; 
    if(_obuf.length() == 0) {
        active_epollout = true;					//若数据已发完则激活写事件，若数据未发完则等写完再激活
    }

    msg_head        head;
    head.msgid      = msgid;
    head.msglen     = msglen;
    int ret = _obuf.send_data((const char *)&head, MSG_HEAD_LEN);
    if (ret != 0) {
        fprintf(stderr, "send head error\n");           //发送失败 是否和删除数据以后不发了
        return -1;
    }
    ret = _obuf.send_data(data, msglen);
    if (ret != 0) {
        _obuf.pop(MSG_HEAD_LEN);				        //若写消息体失败则回滚消息头
        return -1;
    }

    if (active_epollout == true) 
        _loop->add_io_event(_connfd, EPOLLOUT, callback_conn_wt, this);	 //激活EPOLLOUT写事件

    return 0;
}

//销毁tcp_conn
void tcp_conn::clean_conn()
{
    if (tcp_server::conn_close_cb)
        tcp_server::conn_close_cb(this, tcp_server::conn_close_cb_args);    // 若有销毁回调则调用
    
    tcp_server::decr_conn(_connfd);      		    // 删除server记录    
    _loop->del_io_event(_connfd);					// 删除相关事件
    _ibuf.clear(); 							
    _obuf.clear();
   
    int fd = _connfd;			 					// 关闭原始套接字
    _connfd = -1;
    close(fd);
}
