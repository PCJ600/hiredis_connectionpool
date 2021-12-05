#include "hiredis_api.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 256

int DBConnStart(DBConnPool *pool, DBConn **conn);

// Inner API
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
    printf("first request failed, c->err: %d, c->errstr: %s\n", c->err, c->errstr); // info
    redisFree(c);

    int ret = DBConnStart(pool, &conn);
    if (ret != 0) {
        printf("reconnect failed!\n"); // err
        va_end(ap2);
        return NULL;
    }

    reply = redisvCommand(conn->handle, fmt, ap2);
    if (reply == NULL) {
        printf("retry failed after reconnect: err: %d, errstr: %s\n", c->err, c->errstr);  // err
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


// external API
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

// get [key]
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
                strcpy(value, reply->str);  // request success
            }
        } else {
            printf("reply->type: %d", reply->type);  // err
            ret = HIREDIS_REPLY_TYPE_ERR;
        }
    }
    freeReplyObject(reply);
    DBConnRelease(pool, conn);
    return ret;
}

// set [key] [value]
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
    int ret = DBSetString(pool, key, buf);
    if (ret != HIREDIS_OK) {
        return ret;
    }
    return HIREDIS_OK;
}

int DBGetUint32(DBConnPool *pool, const char *key, uint32_t *value)
{
    return DBGetInt32(pool, key, (int32_t *)value);
}

int DBSetUint32(DBConnPool *pool, const char *key, uint32_t value)
{
    char buf[BUFFER_SIZE];
    sprintf(buf, "%u", value);
    int ret = DBSetString(pool, key, buf);
    if (ret != HIREDIS_OK) {
        return ret;
    }
    return HIREDIS_OK;
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
    sprintf(buf, "%lld", value);
    int ret = DBSetString(pool, key, buf);
    if (ret != HIREDIS_OK) {
        return ret;
    }
    return HIREDIS_OK;
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
    sprintf(buf, "%llu", value);
    int ret = DBSetString(pool, key, buf);
    if (ret != HIREDIS_OK) {
        return ret;
    }
    return HIREDIS_OK;
}

