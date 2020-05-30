#include "thread_pool.h"
#include "event_loop.h"
#include "net_tcp__conn.h"
#include <unistd.h>
#include <stdio.h>

/*
 * 处理task消息业务的主流程
 *     调用task_queue:: send()会触发此函数
 */
void deal_task_msg(event_loop *loop, int fd, void *args)
{
    task_queue<task_msg>*     queue = (task_queue<task_msg>*)args;  //得到是哪个消息队列触发的 
    std::queue<task_msg>      tasks;                         //将queue中的全部任务取出来
    
    queue->recv(tasks);
    while (tasks.empty() != true) {
        task_msg task = tasks.front();
        tasks.pop();
        switch (task.type){
            case task_msg::NEW_CONN : {
                    tcp_conn *conn = new tcp_conn(task.connfd, loop);   // 将tcp_conn加入当前线程loop去监听
                    if (conn == NULL) {
                        fprintf(stderr, "in thread new tcp_conn error\n");
                        exit(1);
                    }else
                        printf("[thread:%lx]: new connect !\n",pthread_self());
                } break;
            case task_msg::NEW_TASK :{
                    loop->add_task(task.task_cb, task.args);            // 普通新任务  让当前loop触发task任务的回调
                } break;
            default:
                fprintf(stderr, "unknow task!\n");                  // 其他未识别任务
        }
    }
}

//一个线程的主业务main函数
void *thread_main(void *args)
{
    task_queue<task_msg>  *queue  = (task_queue<task_msg>*)args;
    event_loop            *loop   = new event_loop();             
    if (loop == NULL) {
        fprintf(stderr, "new event_loop error\n");
        exit(1);
    }
    queue->set_loop(loop);                          //每个线程都有一个event_loop监控客户端的读写事件   
    queue->set_callback(deal_task_msg, queue);  	//注册一个触发消息任务读写的callback函数
    loop->event_process();                          //启动阻塞监听
    return NULL;
}

thread_pool::thread_pool(int thread_cnt)
{
    _index          = 0;
    _queues         = NULL;
    _thread_cnt     = thread_cnt;
    if (_thread_cnt <= 0) {
        fprintf(stderr, "_thread_cnt < 0\n");
        exit(1);
    }

    _queues = new task_queue<task_msg>*[thread_cnt];         //任务队列的个数和线程个数一致
    _tids   = new pthread_t[thread_cnt];
    int     ret;
    for (int i = 0; i < thread_cnt; ++i) {
        _queues[i]  = new task_queue<task_msg>();      //创建一个线程并绑定一个任务消息队列
        ret         = pthread_create(&_tids[i], NULL, thread_main, _queues[i]);
        if (ret == -1) {
            perror("thread_pool, create thread");
            exit(1);
        }
        pthread_detach(_tids[i]);        //将线程脱离
        printf("  create %d thread : %lx\n", i, _tids[i]);
    }
}

task_queue<task_msg>* thread_pool::get_thread()
{
    if (++_index == _thread_cnt)
        _index = 0; 
    return _queues[_index];
}

void thread_pool::send_task(task_func func, void *args)
{
    task_msg        task;                               //为每个thread里的pool添加一个task任务
    for (int i = 0; i < _thread_cnt; i++) {
        task.type    = task_msg::NEW_TASK;              //封装一个task消息
        task.task_cb = func;
        task.args    = args;
        task_queue<task_msg> *queue = _queues[i];     //取出第i个thread的消息队列
        queue->send(task);                              //发送task消息
    }
}
