#include "hiredis_connpool.h"
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
            printf("redisConnect failed, redisContext is not NULL!");
            redisFree(c);
        } else {
            printf("redisConnect failed, redisContext is NULL!");
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
        return NULL;
    }

    if (DBConnStart(pool, &conn) != 0) {
        return NULL;
    }
    return conn;
}

// 创建连接池
DBConnPool *DBConnPoolCreate(int max_conn_num)
{
    DBConnPool *p = (DBConnPool *)malloc(sizeof(*p));
    if (p == NULL) {
        return NULL;
    }

    strcpy(p->host, SERV_IP);
    p->port = SERV_PORT;
    p->cur_conn_num = 1; // 初始只给1个连接
    p->max_conn_num = max_conn_num;
    p->conn_timeout_sec = CONN_TIMEOUT_SEC;
    p->conn_timeout_usec = CONN_TIMEOUT_USEC;

    (void)pthread_mutex_init(&p->mutex, NULL);
    (void)pthread_cond_init(&p->cond, NULL);

    p->free_conn_list = listCreate();
    if (p->free_conn_list == NULL) {
        pthread_mutex_destroy(&p->mutex);
        pthread_cond_destroy(&p->cond);
        return NULL;
    }
    listSetFreeMethod(p->free_conn_list, DBConnFree);

    // 初始建立cur_conn_num个连接
    for (int i = 0; i < p->cur_conn_num; ++i) {
        DBConn *conn = DBConnCreate(p);
        if (conn == NULL) {
            listRelease(p->free_conn_list);
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
                assert("DBConGet failed, con't create connection!"); // err
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

