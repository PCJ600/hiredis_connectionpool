#ifndef HIREDIS_CONNPOOL_H
#define HIREDIS_CONNPOOL_H

#include <stdlib.h>
#include <pthread.h>
#include <hiredis/hiredis.h>
#include "list.h"

#define CONNPOOL_TRUE 1
#define CONNPOOL_FALSE 0

#define CONNPOOL_OK 0
#define CONNPOOL_ERR (-1)

#define IPV4_BUF_LEN 20

#define SERV_IP "127.0.0.1"
#define SERV_PORT 6379

typedef struct DBConn {
    redisContext *handle;
} DBConn;


typedef struct DBConnPool {
    char host[IPV4_BUF_LEN];
    int port;
    int cur_conn_num;      // 初始连接个数
    int max_conn_num;      // 最大连接个数
    int conn_timeout_sec;  // 连接超时, 单位: 秒
    int conn_timeout_usec; // 连接超时, 单位: 微妙
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    list *free_conn_list;  // 空闲连接队列
} DBConnPool;


// 创建连接池
DBConnPool *DBConnPoolCreate(int max_conn_num);

// 获取一个可用连接
DBConn *DBConnGet(DBConnPool *pool);

// 释放一个连接
void DBConnRelease(DBConnPool *pool, DBConn *conn);

// 销毁连接池
void DBConnPoolDestroy(DBConnPool *pool);

#endif
