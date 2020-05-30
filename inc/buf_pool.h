#pragma once

#include <ext/hash_map>
#include "buf_io.h"
#include <stdint.h>

typedef __gnu_cxx::hash_map<int, buf_io*> pool_t;

enum MEM_CAP {
    m4K     = 4096,
    m16K    = 16384,
    m64K    = 65536,
    m256K   = 262144,
    m1M     = 1048576,
    m4M     = 4194304,
    m8M     = 8388608
};

//总内存池最大限制 单位是Kb 所以目前限制是 5GB
#define EXTRA_MEM_LIMIT (5U *1024 *1024) 

class buf_pool 
{
public:
    static void init() { _instance = new buf_pool(); }
    static buf_pool *instance();
    buf_io *alloc_buf(int N);
    buf_io *alloc_buf() { return alloc_buf(m4K); }
    void revert(buf_io *buffer);     //重置一个buf_io

private:
    buf_pool();
    buf_pool(const buf_pool&);                   //拷贝构造私有化
    const buf_pool& operator=(const buf_pool&);
    void init_pool(int mem_siz, int mem_cnt);
    pool_t                  _pool;		        //所有buffer的一个map集合句柄
    uint64_t                _total_mem;         //总buffer池的内存大小 单位为KB
    static buf_pool         *_instance;         //
    static pthread_once_t   _once;              //用于保证创建单例的init方法只执行一次的锁
    static pthread_mutex_t  _mutex;             //用户保护内存池链表修改的互斥锁
};
