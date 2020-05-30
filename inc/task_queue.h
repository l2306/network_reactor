#pragma once

#include <queue>
#include <pthread.h>
#include <sys/eventfd.h>
#include <stdio.h>
#include <unistd.h>
#include "event_loop.h"

/*
 * 每个thread对应的 消息任务队列
 */
template <typename T>
class task_queue
{
public:
    task_queue() : _loop(NULL) 
    {
        pthread_mutex_init(&_queue_mtx, NULL);
        _evfd   = eventfd(0, EFD_NONBLOCK);         //用于激活epoll_wait, 辅助完成任务添加的回调执行
        if (_evfd == -1) {
            perror("evenfd(0, EFD_NONBLOCK)");
            exit(1);
        }
    }
    ~task_queue()
    {
        pthread_mutex_destroy(&_queue_mtx);
        close(_evfd);
    }

    //向队列添加一个任务
    void send(const T& task) {
        pthread_mutex_lock(&_queue_mtx);
        unsigned long long idle_num = 1;            //触发消息事件的占位传输内容
        _queue.push(task);                          //将任务添加到队列
        int ret = write(_evfd, &idle_num, sizeof(unsigned long long));  //向_evfd写，触发EPOLLIN事件,来处理该任务
        if (ret == -1)
            perror("_evfd write");
        pthread_mutex_unlock(&_queue_mtx);
    }

    //获取队列，(当前队列已经有任务)
    void recv(std::queue<T>& new_queue) {
        pthread_mutex_lock(&_queue_mtx);  
        unsigned long long idle_num = 1;
        int ret = read(_evfd, &idle_num, sizeof(unsigned long long));  //把占位的数据读出来，确保底层缓冲没有数据存留
        if (ret == -1)
            perror("_evfd read");
        std::swap(new_queue, _queue);
        pthread_mutex_unlock(&_queue_mtx);
    }

    event_loop * get_loop() { return _loop; }
    void set_loop(event_loop *loop) { _loop = loop; } // task_queue对应event_loop
    void set_callback(io_cb *cb, void *args = NULL)   // 设置当前消息任务队列的每个任务触发的回调
    {
        if (_loop != NULL) 
            _loop->add_io_event(_evfd, EPOLLIN, cb, args);
    }
    
private:
    int 			_evfd;          //触发消息任务队列读取的每个消息业务的fd，用于激活loop
    event_loop 		*_loop;    		//当前消息任务队列所绑定在哪个event_loop
    std::queue<T> 	_queue; 		//队列
    pthread_mutex_t _queue_mtx; 	//任务添加、读取的保护锁
};
