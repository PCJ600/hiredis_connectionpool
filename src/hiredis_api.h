#ifndef __HIREDIS_API_H__
#define __HIREDIS_API_H__

#include "hiredis_connpool.h"

int DBConnPoolGetString(DBConnPool *pool, const char *key, char *value);
int DBConnPoolSetString(DBConnPool *pool, const char *key, const char *value);

#endif
