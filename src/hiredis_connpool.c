#include "hiredis_connpool.h"
#include "hiredis_log.h"
#include <assert.h>
#include <string.h>
#include "list.h"


static const int CONN_TIMEOUT_SEC = 5;
static const int CONN_TIMEOUT_USEC = 0;

// 释放一个连接
static void DBConnFree(void *ptr)
{
    DBConn *conn = (DBConn *)ptr;
    if (conn == NULL || conn->handle == NULL) {
        return;
    }
    redisFree(conn->handle);
    free(conn);
}

int DBConnStart(DBConnPool *pool, DBConn **conn)
{
    struct timeval tv;
    tv.tv_sec = pool->conn_timeout_sec;
    tv.tv_usec = pool->conn_timeout_usec;
    redisContext *c = redisConnectWithTimeout(pool->host, pool->port, tv);
    if (c == NULL || c->err) {
        if (c != NULL) {
            HiredisClientLog(LOG_ERR, "redisConnect failed, redisContext is not NULL!\n");
            redisFree(c);
        } else {
            HiredisClientLog(LOG_ERR, "redisConnect failed, redisContext is NULL!\n");
        }
        return -1;
    }
    (*conn)->handle = c;
    return 0;
}


// 创建一个连接
DBConn *DBConnCreate(DBConnPool *pool)
{
    DBConn *conn;
    if ((conn = (DBConn *)malloc(sizeof(*conn))) == NULL) {
        HiredisClientLog(LOG_ERR, "DBConnCreate failed, malloc err!\n");
        return NULL;
    }

    if (DBConnStart(pool, &conn) != 0) {
        HiredisClientLog(LOG_ERR, "DBConnCreate failed, connect err!\n");
        return NULL;
    }
    return conn;
}

// 创建连接池
DBConnPool *DBConnPoolCreate(int max_conn_num)
{
    DBConnPool *p = (DBConnPool *)malloc(sizeof(*p));
    if (p == NULL) {
        HiredisClientLog(LOG_ERR, "DBConnPoolCreate failed, malloc connpool err!\n");
        return NULL;
    }

    strcpy(p->host, SERV_IP); // TODO: pass SERV_IP, SERV_PORT by func parameters
    p->port = SERV_PORT;
    p->cur_conn_num = 1;
    p->max_conn_num = max_conn_num;
    p->conn_timeout_sec = CONN_TIMEOUT_SEC;
    p->conn_timeout_usec = CONN_TIMEOUT_USEC;

    (void)pthread_mutex_init(&p->mutex, NULL);
    (void)pthread_cond_init(&p->cond, NULL);

    p->free_conn_list = listCreate();
    if (p->free_conn_list == NULL) {
        pthread_mutex_destroy(&p->mutex);
        pthread_cond_destroy(&p->cond);
        HiredisClientLog(LOG_ERR, "DBConnPoolCreate failed, free_conn_list is null!\n");
        return NULL;
    }
    listSetFreeMethod(p->free_conn_list, DBConnFree);

    // 初始建立cur_conn_num个连接
    for (int i = 0; i < p->cur_conn_num; ++i) {
        DBConn *conn = DBConnCreate(p);
        if (conn == NULL) {
            listRelease(p->free_conn_list);
            pthread_mutex_destroy(&p->mutex);
            pthread_cond_destroy(&p->cond);
            HiredisClientLog(LOG_ERR, "DBConnPoolCreate failed, initial connection err!\n");
            return NULL;
        }
        listPushBack(p->free_conn_list, conn);
    }
    return p;
}


// 获取一个可用连接
DBConn *DBConnGet(DBConnPool *pool)
{
    pthread_mutex_lock(&pool->mutex);

    while (listisEmpty(pool->free_conn_list) == CONNPOOL_TRUE) {
        if (pool->cur_conn_num >= pool->max_conn_num) {
            pthread_cond_wait(&pool->cond, &pool->mutex);
        } else {
            DBConn *conn = DBConnCreate(pool);
            if (conn == NULL) {
                HiredisClientLog(LOG_ERR, "DBConGet failed, can't create connection!");
                pthread_mutex_unlock(&pool->mutex);
                return NULL;
            }
            listPushBack(pool->free_conn_list, conn);
            ++(pool->cur_conn_num);
        }
    }
    DBConn *conn = listPopFront(pool->free_conn_list);

    pthread_mutex_unlock(&pool->mutex);
    return conn;
}

// 归还一个连接
void DBConnRelease(DBConnPool *pool, DBConn *conn)
{
    pthread_mutex_lock(&pool->mutex);

    listPushBack(pool->free_conn_list, conn);
    pthread_cond_signal(&pool->cond);

    pthread_mutex_unlock(&pool->mutex);
}

// 销毁连接池
void DBConnPoolDestroy(DBConnPool *pool)
{
    if (pool == NULL) {
        return;
    }
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    listRelease(pool->free_conn_list);
    free(pool);
}

