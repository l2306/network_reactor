#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "buf_io.h"


buf_io::buf_io(int size)
{
    capacity = size; 
    len = 0;
    head = 0;
    next = 0;

    data = new char[size];
    assert(data);
}

//清空数据
void buf_io::clear() {
    len = head = 0;
}

//将已经处理过的数据，清空,将未处理的数据提前至数据首地址
void buf_io::adjust() {
    if (head != 0) {
        if (len != 0) {
            memmove(data, data+head, len);
        }
        head = 0;
    }
}

//将其他buf_io对象数据考本到自己中
void buf_io::copy(const buf_io *other) {
    memcpy(data, other->data + other->head, other->len);
    head = 0;
    len = other->len;
}

//处理长度为len的数据，移动head和修正len
void buf_io::pop(int num) {
    len -= num;
    head += num;
}


