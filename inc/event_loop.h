#pragma once
/*
 * event_loop事件处理机制
 */
#include <sys/epoll.h>
#include <ext/hash_map>
#include <ext/hash_set>
#include <vector>
#include "event_base.h"

#include "task_msg.h"

#define MAXEVENTS 10

typedef __gnu_cxx::hash_map<int, io_event>              io_event_map;       // map: fd->io_event 
typedef __gnu_cxx::hash_map<int, io_event>::iterator    io_event_map_it;    //定义指向上面map类型的迭代器
typedef __gnu_cxx::hash_set<int>                        listen_fd_set;      //全部正在监听的fd集合
typedef void (*task_func)(event_loop *loop, void *args);                    //定义异步任务回调函数类型

class event_loop {
    typedef std::pair<task_func, void*> task_func_pair;
public:
    event_loop();                                       //构造，初始化epoll堆
    void event_process();                               //阻塞循环处理事件
    void add_io_event(int fd, int mask,
            io_cb *proc, void *args=NULL);              //添加一个io事件到loop中
    void del_io_event(int fd);                          //删除一个io事件从loop中
    void del_io_event(int fd, int mask);                //删除一个io事件的EPOLLIN/EPOLLOUT
    void get_listen_fds(listen_fd_set &fds)             //获取全部监听事件的fd集合
        { fds = listen_fds; }
                                                        //=== 异步任务task模块需要的方法 ===
    void add_task(task_func func, void *args);          //添加一个任务task到ready_tasks集合中
    void exec_ready_tasks();                            //执行全部的ready_tasks里面的任务
private:
    int                         _epfd;                  //epoll fd
    io_event_map                _io_evs;                //当前event_loop监控的fd和对应事件的关系    
    listen_fd_set               listen_fds;             //当前event_loop一共哪些fd在监听
    struct epoll_event          _fired_evs[MAXEVENTS];  //一次性最大处理的事件
    std::vector<task_func_pair>     _ready_tasks;       //需要被执行的task集合
};
