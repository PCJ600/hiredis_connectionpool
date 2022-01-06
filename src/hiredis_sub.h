#ifndef HIREDIS_SUB_H
#define HIREDIS_SUB_H

#define SUB_BUF_LEN 256

typedef enum _hiredisSubType
{
    HIREDIS_SUBSCRIBE,
    HIREDIS_PSUBSCRIBE
} hiredisSubType;


typedef enum _hiredisSubEventType
{
    HIREDIS_SUB_EVENT_SET,
    HIREDIS_SUB_EVENT_DEL,
    HIREDIS_SUB_EVENT_UNKNOWN
} hiredisSubEventType;


typedef struct _hiredisSubContext
{
    hiredisSubType subType;
    hiredisSubEventType event;
    char pattern[SUB_BUF_LEN];
    char channel[SUB_BUF_LEN];
    void *args;
} hiredisSubContext;


typedef void (*hiredisSubCallback) (hiredisSubContext *cxt);
int HiredisSubscribe(const char *key, hiredisSubCallback cb, hiredisSubContext *cxt);
int HiredisPsubscribe(const char *key, hiredisSubCallback cb, hiredisSubContext *cxt);

#endif
