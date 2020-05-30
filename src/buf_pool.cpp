#include "buf_pool.h"
#include <assert.h>
#include <stdio.h>

buf_pool * buf_pool::_instance = NULL;                       //单例对象

pthread_once_t buf_pool::_once = PTHREAD_ONCE_INIT;            //用于保证创建单例的init方法只执行一次的锁

pthread_mutex_t buf_pool::_mutex = PTHREAD_MUTEX_INITIALIZER;    //用户保护内存池链表修改的互斥锁


//构造函数 主要是预先开辟一定量的空间
//   这里buf_pool是一个hash，每个key都是不同空间容量
//    对应的value是一个buf_io集合的链表
//buf_pool -->  [m4K]   -- buf_io->buf_io->buf_io...
//              [m16K]  -- buf_io->buf_io->buf_io...
//              [m64K]  -- buf_io->buf_io->buf_io...
//              [m256K] -- buf_io->buf_io->buf_io...
//              [m1M]   -- buf_io->buf_io->buf_io...
//              [m4M]   -- buf_io->buf_io->buf_io...
//              [m8M]   -- buf_io->buf_io->buf_io...
buf_pool::buf_pool():_total_mem(0)
{
    init_pool(m4K, 5000);  //----> 开辟4K内存池 , 4K的buf_io 预开5000个，约20MB
    init_pool(m16K, 1000); //约16MB
    init_pool(m64K, 500);  //约32MB
    init_pool(m256K, 200); //约50MB
    init_pool(m1M, 50);    //约50MB
    init_pool(m4M, 20);    //约80MB
}

buf_pool *buf_pool::instance(){        
    pthread_once(&_once, init);   //保证init方法在这个进程执行中 只被执行一次
    return _instance;
}

//创建 定长内存池map，单内存块大小mem_siz,单块内存个数mem_cnt
void buf_pool::init_pool(int mem_siz, int mem_cnt)
{
  buf_io *prev;
    _pool[mem_siz] = new buf_io(mem_siz);
    if (_pool[mem_siz] == NULL) {
        fprintf(stderr, "new buf_io m4K error");
        exit(1);
    }
    prev = _pool[mem_siz];
    for (int i = 1; i < mem_cnt; i ++) {
        prev->next = new buf_io(mem_siz);
        if (prev->next == NULL) {
            fprintf(stderr, "new buf_io m4K error");
            exit(1);
        }
        prev = prev->next;
    }
    _total_mem += mem_siz * mem_cnt / 1024;
}

//开辟一个buf_io
//   1 N个字节的大小的空间，找到与N最接近的buf hash组，取出，
//   2 如果该组已经没有节点使用，可以额外申请
//   3 总申请长度不能够超过最大的限制大小 EXTRA_MEM_LIMIT
//   4 若有该节点需要的内存块，从pool摘除并直接取出
buf_io *buf_pool::alloc_buf(int N) 
{
    
    int index;          //1 找到N最接近哪hash 组
    if (N <= m4K)           index = m4K;
    else if (N <= m16K)     index = m16K;
    else if (N <= m64K)     index = m64K;
    else if (N <= m256K)    index = m256K;
    else if (N <= m1M)      index = m1M;
    else if (N <= m4M)      index = m4M;
    else if (N <= m8M)      index = m8M;
    else                    return NULL;

    //2 如果该组已经没有，需要额外申请，那么需要加锁保护
    pthread_mutex_lock(&_mutex);
    if (_pool[index] == NULL) {
        if (_total_mem + index/1024 >= EXTRA_MEM_LIMIT) {         //当前的开辟的空间已经超过最大限制
            fprintf(stderr, "already use too many memory!\n");
            exit(1);
        }
        buf_io *new_buf = new buf_io(index);
        if (new_buf == NULL) {
            fprintf(stderr, "new buf_io error\n");
            exit(1);
        }
        _total_mem += index/1024;
        pthread_mutex_unlock(&_mutex);
        return new_buf;
    }

    buf_io *target = _pool[index];		//3 从pool中摘除该内存块
    _pool[index] = target->next;
    pthread_mutex_unlock(&_mutex);
    target->next = NULL;
    return target;
}

//重置一个buf_io,将一个buf 上层不再使用，或者使用完成之后，需要将该buf放回pool中
void buf_pool::revert(buf_io *buffer)
{
    int index = buffer->capacity;       //每个buf的容量都是固定的 在hash的key中取值
    buffer->len = 0;                 //重置buf_io中的内置位置指针
    buffer->head = 0;

    pthread_mutex_lock(&_mutex);
    assert(_pool.find(index) != _pool.end());      //找到对应的hash组 buf首届点地址
    buffer->next = _pool[index];			          //将buffer插回链表头部
    _pool[index] = buffer;
    pthread_mutex_unlock(&_mutex);
}
