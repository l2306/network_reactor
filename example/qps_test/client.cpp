#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <iostream>
#include "net_tcp_client.h"
#include "msg_echo.pb.h"

struct Qps
{
    Qps() {
        last_time = time(NULL); 
        succ_cnt = 0;
    }

    long last_time;//最后一次发包时间 ms为单位
    int succ_cnt; //成功收到服务器回显的次数
};


//客户端业务
void busi(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data)
{
    Qps                 *qps = (Qps*)user_data; //用户参数
    qps_test::Msg_echo  request, response;
    
    if (response.ParseFromArray(data, len) == false) {      //解析服务端传来的pb数据
        printf("server call back data error\n");
        return;
    }    
    if (response.id() == 1) {            //判断数据内容是否回显一致
        qps->succ_cnt ++;                                   //服务器请求响应成功一次
    }

    long current_time = time(NULL);
    if (current_time - qps->last_time >= 1) {
                                                            //如果当前时间比最后记录时间大于1秒，那么我们进行记录
        printf("---> qps = %d <----\n", qps->succ_cnt);
        qps->last_time = current_time;                      //记录最后时间
        qps->succ_cnt = 0;                                  //清空成功次数
    }

    request.set_id(response.id() + 1);
    request.set_content(response.content());
    std::string strReq;
    request.SerializeToString(&strReq);

	std::cout<<" rcv S"<< request.content()<<std::endl;

    conn->snd_msg(strReq.c_str(), strReq.size(), msgid);      //给服务端发送新的请求
}


//创建链接成功之后
void connection_start(net_connection *client, void *args)
{
    qps_test::Msg_echo request;
    std::string strReq;

    request.set_id(1);
    request.set_content("Hello Lars!!!  @client signed@ ");
    request.SerializeToString(&strReq);

    int msgid = 1;//与server端的消息路由一致
    client->snd_msg(strReq.c_str(), strReq.size(), msgid);
}


void *thread_main(void *args)
{
    //给服务端发包
     
    event_loop loop; 

    tcp_client client(&loop, "127.0.0.1", 7777, "qps client");

    Qps qps;

    //设置回调
    client.add_msg_router(1, busi, (void*)&qps);

    //设置链接创建成功之后Hook
    client.set_conn_start(connection_start);

    loop.event_process();

    return NULL;
}



int main(int argc, char **argv) 
{
    if (argc == 1) {
        printf("Usage: ./client [threadNum]\n");
        return 1;
    }

    //创建N个线程
    int         thread_num  = atoi(argv[1]);
    pthread_t   *tids       = new pthread_t[thread_num];

    for (int i = 0; i < thread_num; i++)
        pthread_create(&tids[i], NULL, thread_main, NULL);

    for (int i = 0; i < thread_num; i++)
        pthread_join(tids[i], NULL);

    return 0;
}
