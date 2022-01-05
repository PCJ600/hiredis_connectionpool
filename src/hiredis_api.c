#include "hiredis_api.h"
#include "hiredis_log.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 256

int DBConnStart(DBConnPool *pool, DBConn **conn);

// 如果请求失败, 可能是因为服务端设置了连接超时, 主动关闭连接
// 客户端检测到这种场景，需要重新发起一次连接请求, 而不是直接返回出错
static void *DBConnvCommand(DBConnPool *pool, DBConn *conn, const char *fmt, va_list ap)
{
    va_list ap2;
    va_copy(ap2, ap);

    void *reply = redisvCommand(conn->handle, fmt, ap);
    if (reply != NULL) {
        return reply;       
    }

    redisContext *c = conn->handle;
    HiredisClientLog(LOG_WARNING, "first request failed, c->err: %d, c->errstr: %s\n", c->err, c->errstr);
    redisFree(c);

    int ret = DBConnStart(pool, &conn);
    if (ret != 0) {
        HiredisClientLog(LOG_ERR, "reconnect failed!\n");
        va_end(ap2);
        return NULL;
    }

    reply = redisvCommand(conn->handle, fmt, ap2);
    if (reply == NULL) {
        HiredisClientLog(LOG_ERR, "retry failed after reconnect: err: %d, errstr: %s\n", c->err, c->errstr);
    }
    va_end(ap2);
    return reply;
}

static void *DBConnCommand(DBConnPool *pool, DBConn *conn, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    void *reply = DBConnvCommand(pool, conn, fmt, ap);
    va_end(ap);
    return reply;
}

int DBGenericCommand(DBConnPool *pool, const char *fmt, ...)
{
    DBConn *conn = DBConnGet(pool);
    if (conn == NULL) {
        return HIREDIS_DBCONN_NIL;
    }

    va_list ap;
    va_start(ap, fmt);
    void *reply = DBConnvCommand(pool, conn, fmt, ap);
    va_end(ap);
    
    int ret = CONNPOOL_OK;
    if (reply == NULL) {
        ret = HIREDIS_REPLY_NIL;
    }
    freeReplyObject(reply);
    DBConnRelease(pool, conn);
    return ret;
}

// redis-cli get [key]
int DBGetString(DBConnPool *pool, const char *key, char *value)
{
    DBConn *conn = DBConnGet(pool);
    if (conn == NULL) {
        return HIREDIS_DBCONN_NIL;
    }

    int ret = CONNPOOL_OK;
    redisReply *reply = (redisReply *)DBConnCommand(pool, conn, "GET %s", key);
    if (reply == NULL) {
        ret = HIREDIS_REPLY_NIL;
    } else {
        if (reply->type == REDIS_REPLY_STRING) {
            if (reply->str == NULL) {
                ret = HIREDIS_REPLY_STR_NIL;
            } else {
                strcpy(value, reply->str);
            }
        } else {
            HiredisClientLog(LOG_ERR, "get string failed, reply->type: %d\n", reply->type);
            ret = HIREDIS_REPLY_TYPE_ERR;
        }
    }
    freeReplyObject(reply);
    DBConnRelease(pool, conn);
    return ret;
}

// redis-cli set [key] [value]
int DBSetString(DBConnPool *pool, const char *key, const char *value)
{
    DBConn *conn = DBConnGet(pool);
    if (conn == NULL) {
        return HIREDIS_DBCONN_NIL;
    }

    int ret = CONNPOOL_OK;
    redisReply *reply = (redisReply *)DBConnCommand(pool, conn, "SET %s %s", key, value);
    if (reply == NULL) {
        ret = HIREDIS_REPLY_NIL;
    } else if (strcmp(reply->str, "OK") != 0) {
        ret = HIREDIS_REPLY_SET_FAILED;
    } else {
        ret = HIREDIS_OK;
    }

    freeReplyObject(reply);
    DBConnRelease(pool, conn);
    return ret;
}

int DBGetInt32(DBConnPool *pool, const char *key, int32_t *value)
{
    char buf[BUFFER_SIZE];
    int ret = DBGetString(pool, key, buf);
    if (ret != HIREDIS_OK) {
        return ret;
    }
    *value = atoi(buf);
    return HIREDIS_OK;
}

int DBSetInt32(DBConnPool *pool, const char *key, int32_t value)
{
    char buf[BUFFER_SIZE];
    sprintf(buf, "%d", value);
    return DBSetString(pool, key, buf);
}

int DBGetUint32(DBConnPool *pool, const char *key, uint32_t *value)
{
    return DBGetInt32(pool, key, (int32_t *)value);
}

int DBSetUint32(DBConnPool *pool, const char *key, uint32_t value)
{
    char buf[BUFFER_SIZE];
    sprintf(buf, "%u", value);
    return DBSetString(pool, key, buf);
}

int DBGetInt64(DBConnPool *pool, const char *key, int64_t *value)
{
    char buf[BUFFER_SIZE];
    int ret = DBGetString(pool, key, buf);
    if (ret != HIREDIS_OK) {
        return ret;
    }
    *value = strtoll(buf, NULL, 10);
    return HIREDIS_OK;
}

int DBSetInt64(DBConnPool *pool, const char *key, int64_t value)
{
    char buf[BUFFER_SIZE];
    sprintf(buf, "%ld", value);
    return DBSetString(pool, key, buf);
}

int DBGetUint64(DBConnPool *pool, const char *key, uint64_t *value)
{
    char buf[BUFFER_SIZE];
    int ret = DBGetString(pool, key, buf);
    if (ret != HIREDIS_OK) {
        return ret;
    }
    *value = strtoull(buf, NULL, 10);
    return HIREDIS_OK;
}

int DBSetUint64(DBConnPool *pool, const char *key, uint64_t value)
{
    char buf[BUFFER_SIZE];
    sprintf(buf, "%lu", value);
    return DBSetString(pool, key, buf);
}

// 进程级hiredis连接池封装
static DBConnPool *g_connPool = NULL;

void __attribute__((constructor)) HiredisConnPoolInit()
{
    static const int MAX_NUM_CONNECTIONS_PER_CONNPOOL = 10;
    g_connPool = DBConnPoolCreate(MAX_NUM_CONNECTIONS_PER_CONNPOOL);
    assert(g_connPool != NULL);
}

int HiredisGenericCommand(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = DBGenericCommand(g_connPool, fmt, ap);
    va_end(ap);
    return ret;
}

int HiredisGetString(const char *key, char *value)
{
    return DBGetString(g_connPool, key, value);
}

int HiredisSetString(const char *key, const char *value)
{
    return DBSetString(g_connPool, key, value);
}

int HiredisGetUint32(const char *key, uint32_t *value)
{
    return DBGetUint32(g_connPool, key, value);
}

int HiredisSetUint32(const char *key, uint32_t value)
{
    return DBSetUint32(g_connPool, key, value);
}

int HiredisGetInt32(const char *key, int32_t *value)
{
    return DBGetInt32(g_connPool, key, value);
}

int HiredisSetInt32(const char *key, int32_t value)
{
    return DBSetInt32(g_connPool, key, value);
}

int HiredisGetUint64(const char *key, uint64_t *value)
{
    return DBGetUint64(g_connPool, key, value);
}

int HiredisSetUint64(const char *key, uint64_t value)
{
    return DBSetUint64(g_connPool, key, value);
}

int HiredisGetInt64(const char *key, int64_t *value)
{
    return DBGetInt64(g_connPool, key, value);
}

int HiredisSetInt64(const char *key, int64_t value)
{
    return DBSetInt64(g_connPool, key, value);
}
