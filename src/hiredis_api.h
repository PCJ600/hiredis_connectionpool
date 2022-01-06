#ifndef __HIREDIS_API_H__
#define __HIREDIS_API_H__

#include <stdint.h>

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

#endif
