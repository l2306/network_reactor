#pragma once

#include <pthread.h>
#include "task_msg.h"
#include "task_queue.h"

class thread_pool
{
public:
    thread_pool(int thread_cnt);                        //构造，初始化线程池
    task_queue<task_msg>* get_thread();                 //获取一个thead
    void send_task(task_func func, void *args = NULL);  //发送task任务给所有thread
private:
    task_queue<task_msg> **_queues;                     //_queues是当前thread_pool全部的消息任务队列头指针
    int             _thread_cnt;                        //当前线程池中的线程个数
    pthread_t       *_tids;                             //已经启动的全部therad编号
    int             _index;                             //当前选中的线程队列下标
};
