#ifndef __HIREDIS_API_H__
#define __HIREDIS_API_H__

#include "hiredis_connpool.h"

enum HIREDIS_STATE {
    HIREDIS_OK = 0,
    HIREDIS_DBCONN_NIL,
    HIREDIS_REPLY_NIL,
    HIREDIS_REPLY_STR_NIL,
    HIREDIS_REPLY_TYPE_ERR,
    HIREDIS_REPLY_SET_FAILED,
    HIREDIS_STR_TO_INT32_FAILED,
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


// TODO: pubsub API

#endif
