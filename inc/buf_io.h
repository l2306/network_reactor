#pragma once

class buf_io
{
public:
    buf_io(int size);                 //构造，创建一个buf_io对象
    void clear();                     //清空数据
    void adjust();                    //将已处理数据清空,将未处理数据提前至数据首地址
    void copy(const buf_io *other);   //将other数据考本到自己中
    void pop(int len);                //处理长度为len的数据，移动head和修正len

    buf_io *next;                //如果存在多个buffer，是采用链表的形式链接起来
    int     capacity;            //缓存容量大小
    int     len;              	 //有效数据长度
    int     head;                //未处理数据的头部位置索引
    char   *data;                //当前buf_io所保存的数据地址
};

