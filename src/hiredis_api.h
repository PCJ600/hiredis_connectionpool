#ifndef __HIREDIS_API_H__
#define __HIREDIS_API_H__

#include "hiredis_connpool.h"
#include <async.h>

enum HIREDIS_STATE {
    HIREDIS_OK = 0,
    HIREDIS_DBCONN_NIL,
    HIREDIS_REPLY_NIL,
    HIREDIS_REPLY_STR_NIL,
    HIREDIS_REPLY_TYPE_ERR,
    HIREDIS_REPLY_SET_FAILED,
    HIREDIS_STR_TO_INT32_FAILED,
    HIREDIS_PTHREAD_CREATE_FAILED,
    HIREDIS_MALLOC_FAILED,
    HIREDIS_OTHER_ERR,
};

int DBGenericCommand(DBConnPool *pool, const char *fmt, ...);

int DBGetString(DBConnPool *pool, const char *key, char *value);
int DBSetString(DBConnPool *pool, const char *key, const char *value);

int DBGetUint32(DBConnPool *pool, const char *key, uint32_t *value);
int DBSetUint32(DBConnPool *pool, const char *key, uint32_t value);

int DBGetInt32(DBConnPool *pool, const char *key, int32_t *value);
int DBSetInt32(DBConnPool *pool, const char *key, int32_t value);

int DBGetUint64(DBConnPool *pool, const char *key, uint64_t *value);
int DBSetUint64(DBConnPool *pool, const char *key, uint64_t value);

int DBGetInt64(DBConnPool *pool, const char *key, int64_t *value);
int DBSetInt64(DBConnPool *pool, const char *key, int64_t value);


// hide pool, one process only need one connection pool.
int HiredisGenericCommand(const char *fmt, ...);

int HiredisGetString(const char *key, char *value);
int HiredisSetString(const char *key, const char *value);

int HiredisGetUint32(const char *key, uint32_t *value);
int HiredisSetUint32(const char *key, uint32_t value);

int HiredisGetInt32(const char *key, int32_t *value);
int HiredisSetInt32(const char *key, int32_t value);

int HiredisGetUint64(const char *key, uint64_t *value);
int HiredisSetUint64(const char *key, uint64_t value);

int HiredisGetInt64(const char *key, int64_t *value);
int HiredisSetInt64(const char *key, int64_t value);


int DBPSubscribeKeyspaceEvent(const char *pattern, redisCallbackFn *fn, void *args);
#endif
