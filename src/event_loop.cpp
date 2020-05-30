#include "event_loop.h"
#include <assert.h>
#include <stdio.h>
#include <errno.h>


//构造，初始化epoll堆
event_loop::event_loop() 
{
    _epfd = epoll_create1(0);       //flag=0 等价于epll_craete
    if (_epfd == -1) {
        fprintf(stderr, "epoll_create error\n");
        exit(1);
    }
}

//阻塞循环处理事件
void event_loop::event_process()
{
    while (true) {
        io_event_map_it     ev_it;
        int nfds = epoll_wait(_epfd, _fired_evs, MAXEVENTS, 10);
        for (int i = 0; i < nfds; i++) {
            ev_it = _io_evs.find(_fired_evs[i].data.fd);			//通过触发的fd找到对应的绑定事件
            assert(ev_it != _io_evs.end());
            io_event        *ev = &(ev_it->second);
            if (_fired_evs[i].events & EPOLLIN) {						//读事件，掉读回调函数
                ev->rd_cb(this, _fired_evs[i].data.fd, ev->rcb_args);
            } else if (_fired_evs[i].events & EPOLLOUT) {			    //写事件，掉写回调函数
                ev->wr_cb(this, _fired_evs[i].data.fd, ev->wcb_args);
            } else if (_fired_evs[i].events &(EPOLLHUP|EPOLLERR)) {		//水平触发未处理，可能会出现HUP事件，正常处理读写，没有则清空
                if (ev->rd_cb != NULL) {
                    ev->rd_cb(this, _fired_evs[i].data.fd, ev->rcb_args);
                } else if (ev->wr_cb != NULL) {
                    ev->wr_cb(this, _fired_evs[i].data.fd, ev->wcb_args);
                } else {		 //删除
                    fprintf(stderr, "fd %d get error, del it from epoll\n", _fired_evs[i].data.fd);
                    this->del_io_event(_fired_evs[i].data.fd);
                }
            }
        }
        //fprintf(stdout, "%s,%d \n",__FUNCTION__, __LINE__);
        this->exec_ready_tasks();         //每次处理完一组epoll_wait触发的事件之后，处理异步任务
    }
}

/*
 * 处理的事件机制是  （mask中只能赋值EPOLLIN|EPOLLOUT其中之一）
 * 若想注册 EPOLLIN|EPOLLOUT， 需调add_io_event()两次来注册。
 */
//添加一个io事件到loop中
void event_loop::add_io_event(int fd, int mask, io_cb *proc, void *args)
{
    int final_mask;
    int op;

    io_event_map_it   it = _io_evs.find(fd);        //找 当前fd是否已经有事件
    if (it == _io_evs.end()) {						//无则ADD，有则MOD
        final_mask = mask;    
        op = EPOLL_CTL_ADD;
    } else {
        final_mask = it->second.mask | mask;										
        op = EPOLL_CTL_MOD;
    }
    												//注册回调函数
    if (mask & EPOLLIN) {								
        _io_evs[fd].rd_cb    = proc;
        _io_evs[fd].rcb_args = args;
    } else if (mask & EPOLLOUT) {
        _io_evs[fd].wr_cb    = proc;
        _io_evs[fd].wcb_args = args;
    }
    
    _io_evs[fd].mask    = final_mask;
    struct epoll_event  event;				 		//创建原生epoll事件
    event.events        = final_mask;			
    event.data.fd       = fd;
    if (epoll_ctl(_epfd, op, fd, &event) == -1) {
        fprintf(stderr, "epoll ctl %d error\n", fd);
        return;
    }
    listen_fds.insert(fd);				   			// 将fd添加到监听集合中
}

//删除一个io事件从loop中
void event_loop::del_io_event(int fd)
{
    _io_evs.erase(fd);								//将事件从_io_evs删除
    listen_fds.erase(fd);							//将fd从监听集合中删除
    epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);		//将fd从epoll堆删除
}

//删除一个io事件的EPOLLIN/EPOLLOUT
void event_loop::del_io_event(int fd, int mask)
{
    io_event_map_it it = _io_evs.find(fd);			//如果没有该事件，直接返回
    if (it == _io_evs.end())
        return ;
    int &o_mask = it->second.mask;		
    o_mask      = o_mask & (~mask);		    			//修正mask
    if (o_mask == 0) {					 			    //如果修正之后 mask为0，则删除
        this->del_io_event(fd);			
    } else {							      			//如果修正之后，mask非0，则修改
        struct epoll_event  event;
        event.events        = o_mask;
        event.data.fd       = fd;
        epoll_ctl(_epfd, EPOLL_CTL_MOD, fd, &event);
    }
}

//添加一个任务task到ready_tasks集合中
void event_loop::add_task(task_func func, void *args)
{
    task_func_pair func_pair(func, args);
    _ready_tasks.push_back(func_pair);
}

//执行全部的ready_tasks里面的任务
void event_loop::exec_ready_tasks()
{
    //printf("%s _ready_tasks.size()=%d",__FUNCTION__, (int)_ready_tasks.size());
    std::vector<task_func_pair>::iterator it;
    for (it = _ready_tasks.begin(); it != _ready_tasks.end(); it++) {
        task_func func  = it->first;			//任务回调函数
        void *args      = it->second;			//回调函数形参
        func(this, args);			   		    //执行任务
    }
    _ready_tasks.clear();					//全部执行完毕，清空当前的_ready_tasks
}
