#include "hiredis_api.h"
#include <assert.h>
#include <string.h>

int DBConnPoolGetString(DBConnPool *pool, const char *key, char *value)
{
    DBConn *conn = DBConnGet(pool);
    if (conn == NULL) {
        return CONNPOOL_ERR;
    }

    int ret = CONNPOOL_OK;
    char cmd[256];
    sprintf(cmd, "GET %s", key);
    redisContext *c = conn->handle;
    redisReply *reply = (redisReply *)redisCommand(c, cmd);
    if (reply == NULL) {
        ret = CONNPOOL_ERR;
    } else {
        if (reply->type == REDIS_REPLY_STRING) {
            if (reply->str == NULL) {
                ret = CONNPOOL_ERR;
            } else {
                strcpy(value, reply->str);
            }
        } else {
            ret = CONNPOOL_ERR;
        }
    }

    freeReplyObject(reply);
    DBConnRelease(pool, conn);
    return ret;
}

int DBConnPoolSetString(DBConnPool *pool, const char *key, const char *value)
{
    DBConn *conn = DBConnGet(pool);
    if (conn == NULL) {
        return CONNPOOL_ERR;
    }

    int ret = CONNPOOL_OK;
    char cmd[256];
    sprintf(cmd, "SET %s %s", key, value);
    redisContext *c = conn->handle;
    redisReply *reply = (redisReply *)redisCommand(c, cmd);
    if (reply == NULL) {
        ret = CONNPOOL_ERR;
    } else if (!strcmp(reply->str, "OK")) {
        ret = CONNPOOL_ERR;
    }

    freeReplyObject(reply);
    DBConnRelease(pool, conn);
    return ret;
}
