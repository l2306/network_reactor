#pragma once

/*
 * 定义一些IO复用机制或者其他异常触发机制的事件封装
*/
class event_loop;

//IO事件触发的回调函数
typedef void io_cb(event_loop *loop, int fd, void *args);

//封装一次IO触发实现 
struct io_event         
{
    io_event()   :rd_cb(NULL),wr_cb(NULL),rcb_args(NULL),wcb_args(NULL) {}

    int            mask;               	//EPOLLIN EPOLLOUT
    io_cb         *rd_cb;      			//EPOLLIN事件 触发的回调 
    io_cb         *wr_cb;     			//EPOLLOUT事件 触发的回调
    void          *rcb_args;           	//rd_cb的回调函数参数    //连接的指针
    void          *wcb_args;           	//wr_cb的回调函数参数    //连接的指针
};
