#include "net_tcp_client.h"
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/ioctl.h>
#include "message.h"


static void wr_cb(event_loop *loop, int fd, void *args)
{
    ((tcp_client *)args)->do_write();
}

static void rd_cb(event_loop *loop, int fd, void *args)
{
    ((tcp_client *)args)->do_read();
}

tcp_client::tcp_client(event_loop *loop, const char *ip, unsigned short port, const char *name):
    _conn_start_cb(NULL),
    _conn_start_cb_args(NULL),
    _conn_close_cb(NULL),
    _conn_close_cb_args(NULL),
    _obuf(4194304),
    _ibuf(4194304)
{
    _sockfd = -1;
    _name = name;
    _loop = loop;
    bzero(&_server_addr, sizeof(_server_addr));
    _server_addr.sin_family = AF_INET; 
    inet_aton(ip, &_server_addr.sin_addr);
    _server_addr.sin_port = htons(port);
    _addrlen = sizeof(_server_addr);

    this->do_connect();
}

int tcp_client::get_fd()
{
    return _sockfd;
}

//判断链接是否是创建链接，主要是针对非阻塞socket 返回EINPROGRESS错误
static void connection_delay(event_loop *loop, int fd, void *args)
{
    tcp_client *cli = (tcp_client*)args;
    loop->del_io_event(fd);

    int result = 0;
    socklen_t result_len = sizeof(result);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &result, &result_len);
    if (result == 0) {												//链接成功
        cli->connected = true;
        printf("connect %s:%d succ!\n", inet_ntoa(cli->_server_addr.sin_addr), ntohs(cli->_server_addr.sin_port));
        if (cli->_conn_start_cb != NULL) 								//调用注册的创建回调
            cli->_conn_start_cb(cli, cli->_conn_start_cb_args);
        loop->add_io_event(fd, EPOLLIN, rd_cb, cli);
        if (cli->_obuf.len != 0) 
            loop->add_io_event(fd, EPOLLOUT, wr_cb, cli);		//输出可写缓冲数据 
    } else {			 											//链接失败
        fprintf(stderr, "connection %s:%d error\n", inet_ntoa(cli->_server_addr.sin_addr), ntohs(cli->_server_addr.sin_port));
    }
}

//创建链接
void tcp_client::do_connect()
{
    if (_sockfd != -1) 
        close(_sockfd);
    _sockfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, IPPROTO_TCP);
    if (_sockfd == -1) {
        fprintf(stderr, "create tcp client socket error\n");
        exit(1);
    }

    int ret = connect(_sockfd, (const struct sockaddr*)&_server_addr, _addrlen);
    if (ret == 0) {									 
        connected = true; 
        if (_conn_start_cb != NULL)											//调用创建回调
            _conn_start_cb(this, _conn_start_cb_args);
        _loop->add_io_event(_sockfd, EPOLLIN, rd_cb, this);			//注册读回调
        if (this->_obuf.len != 0) {
            _loop->add_io_event(_sockfd, EPOLLOUT, wr_cb, this); 	//若写缓冲有数据，则也需触发写回调
        }
        printf("connect %s:%d succ!\n", inet_ntoa(_server_addr.sin_addr), ntohs(_server_addr.sin_port));
    } else {
        if(errno == EINPROGRESS) {		   //fd非阻塞可能报该错,但并非创建失败 //若fd可写，则创建成功.
            fprintf(stderr, "do_connect EINPROGRESS\n");
            _loop->add_io_event(_sockfd, EPOLLOUT, connection_delay, this);	//让event_loop去触发一个创建判断链接业务 用EPOLLOUT事件立刻触发
        } else {
            fprintf(stderr, "connection error\n");
            exit(1);
        }
    }
}

//主动发送message方法
int tcp_client::snd_msg(const char *data, int msglen, int msgid)
{
    if (connected == false) {
        fprintf(stderr, "no connected, send message stop!\n");
        return -1;
    }
    if (msglen + MSG_HEAD_LEN > this->_obuf.capacity - _obuf.len) {
        fprintf(stderr, "No more space to Write socket!\n");
        return -1;
    }

    msg_head 		head;			//封装消息头
    head.msgid 		= msgid;
    head.msglen 	= msglen;
    memcpy(_obuf.data + _obuf.len, &head, MSG_HEAD_LEN);
    _obuf.len 		+= MSG_HEAD_LEN;
    memcpy(_obuf.data + _obuf.len, data, msglen);
    _obuf.len 		+= msglen;

    bool need_add_event = (_obuf.len == 0) ? true:false;            //是否需要添加写事件触发
    if (need_add_event)                                             //如果obuf中有数据，没必要添加，如果没有数据，添加完数据需要触发
        _loop->add_io_event(_sockfd, EPOLLOUT, wr_cb, this);

    return 0;
}

//处理读业务
int tcp_client::do_read()
{
    assert(connected == true);								
   
    int need_read = 0;										//得到缓冲区里有多少字节要被读取，然后将字节数放入b里面。
    if (ioctl(_sockfd, FIONREAD, &need_read) == -1) {
        fprintf(stderr, "ioctl FIONREAD error");
        return -1;
    }
    assert(need_read <= _ibuf.capacity - _ibuf.len);		  //确保_buf可以容纳可读数据
    
    int ret;
    do {
        ret = read(_sockfd, _ibuf.data + _ibuf.len, need_read);
    } while(ret == -1 && errno == EINTR);

    if (ret == 0) {		 //对端关闭
        if (_name != NULL)
            printf("%s C: connection close by peer!\n", _name);
        else
            printf(" C: connection close by peer!\n");
        clean_conn();
        return -1;
    } else if (ret == -1) {
        fprintf(stderr, " C: do_read() , error\n");
        clean_conn();
        return -1;
    }
    assert(ret == need_read);
    _ibuf.len += ret;

    msg_head    head;
    int         msgid, len;
    while (_ibuf.len >= MSG_HEAD_LEN) {
        memcpy(&head, _ibuf.data + _ibuf.head, MSG_HEAD_LEN);
        msgid 		= head.msgid; 
        len 		= head.msglen;
        //if (len + MSG_HEAD_LEN < _ibuf.len) 
        //    break;
        _ibuf.pop(MSG_HEAD_LEN);											//头部读取完毕
        this->_router.call(msgid, len, _ibuf.data + _ibuf.head, this);		//消息路由分发
        _ibuf.pop(len);														//数据区域处理完毕
    }
    
    _ibuf.adjust();									    //重置head指针
    return 0;
}

//处理写业务
int tcp_client::do_write()
{
    assert(_obuf.head == 0 && _obuf.len);			 		//数据有长度，切头部索引是起始位置
    int ret;
	
    while (_obuf.len) {
        do {												//写数据
            ret = write(_sockfd, _obuf.data, _obuf.len);
        } while(ret == -1 && errno == EINTR);				//非阻塞异常继续重写
        if (ret > 0) {
           _obuf.pop(ret);
           _obuf.adjust();
        }  else if (ret == -1 && errno != EAGAIN) {
            fprintf(stderr, "tcp client write \n");
            this->clean_conn();
        } else {
            break;											//出错,不能再继续写
        }
    }

    if (_obuf.len == 0) {
        printf("do write over, del EPOLLOUT\n");
        this->_loop->del_io_event(_sockfd, EPOLLOUT);				//已经写完，删除写事件
    }
    return 0;
}


//释放链接资源,重置连接
void tcp_client::clean_conn()
{
    if (_sockfd != -1) {
        printf("clean conn, del socket!\n");
        _loop->del_io_event(_sockfd);
        close(_sockfd);
    }
    connected = false;
    if (_conn_close_cb != NULL) 					//调用开发者注册的销毁链接之前触发的Hook
        _conn_close_cb(this, _conn_close_cb_args);
  
    this->do_connect();								    //重新连接
}

tcp_client::~tcp_client()
{
    close(_sockfd);
}
