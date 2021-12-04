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
};


int DBGetString(DBConnPool *pool, const char *key, char *value);
int DBSetString(DBConnPool *pool, const char *key, const char *value);
int DBGenericCommand(DBConnPool *pool, const char *fmt, ...);

#endif
