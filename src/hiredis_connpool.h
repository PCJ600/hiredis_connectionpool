#ifndef HIREDIS_CONNPOOL_H
#define HIREDIS_CONNPOOL_H

#include <stdlib.h>
#include <pthread.h>
#include "hiredis/hiredis.h"
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
    int cur_conn_num;
    int max_conn_num;
    int conn_timeout_sec;
    int conn_timeout_usec;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    list *free_conn_list;
} DBConnPool;


// create connpool
DBConnPool *DBConnPoolCreate(int max_conn_num);

// get one free conn
DBConn *DBConnGet(DBConnPool *pool);

// release one connection
void DBConnRelease(DBConnPool *pool, DBConn *conn);

// destroy connpool
void DBConnPoolDestroy(DBConnPool *pool);

// connect redis based on existed DBConn
int DBConnStart(DBConnPool *pool, DBConn **conn);

#endif
