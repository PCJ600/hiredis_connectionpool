#include "hiredis_sub.h"
#include "hiredis_log.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "hiredis.h"
#include <unistd.h>

#define SUB_DEFAULT_DBID 0

// TODO: remove these fucking hardcoding!
#define SERV_IP "127.0.0.1"
#define SERV_PORT 6379
static const int CONN_TIMEOUT_SEC = 5;
static const int CONN_TIMEOUT_USEC = 0;

enum HIREDIS_SUB_STATE {
    HIREDIS_SUB_OK = 0,
    HIREDIS_SUB_CONTEXT_IS_NIL,
    HIREDIS_SUB_CALLBACK_IS_NIL,
    HIREDIS_SUB_CONNECT_ERR,
    HIREDIS_SUB_REPLY_IS_NIL,
    HIREDIS_SUB_PTHREAD_CREATE_FAILED,
};

typedef struct _SubThreadArgs {
    redisContext *c;
    hiredisSubCallback cb;
    hiredisSubContext *cxt;
} SubThreadArgs;


static hiredisSubEventType _GetEventType(char *event)
{
    if (strcmp(event, "set") == 0) {
        return HIREDIS_SUB_EVENT_SET;
    }
    if (strcmp(event, "del") == 0) {
        return HIREDIS_SUB_EVENT_DEL;
    }
    return HIREDIS_SUB_EVENT_UNKNOWN;
}


static void *SubThread(void *args)
{
    pthread_detach(pthread_self());
    SubThreadArgs *t = (SubThreadArgs *)args;
    redisContext *c = t->c;
    hiredisSubContext *subCxt = t->cxt;
    
    void *reply;
    while(1) {
        if (redisGetReply(c, &reply) != REDIS_OK) {
            HiredisClientLog(LOG_ERR, "SubThread getreply failed, c->err: %d!\n", c->err);
            goto failed;
        }
        
        char *eventType;
        redisReply *_r = (redisReply *)reply;
        if (subCxt->subType == HIREDIS_SUBSCRIBE) { // SUBSCRIBE, reply->element: [message, channel, eventType]
            if (_r->elements != 3) {
                HiredisClientLog(LOG_ERR, "subscribe reply error, len(elements) should be 3, but now is %d!\n", _r->elements);
                goto failed;
            }
            eventType = ((redisReply *)reply)->element[2]->str;
            subCxt->event = _GetEventType(eventType);

        } else { // PSUBSCRIBE reply->element: [pmessage, pattern, channel, eventType]
            if (_r->elements != 4) {
                HiredisClientLog(LOG_ERR, "psubscribe reply error, len(elements) should be 4, but now is %d!\n", _r->elements);
                goto failed;
            }
            char *channel = ((redisReply *)reply)->element[2]->str;
            strcpy(subCxt->channel, channel);
            eventType = ((redisReply *)reply)->element[3]->str;
            subCxt->event = _GetEventType(eventType);
        }

        freeReplyObject(reply);
        // do callback !
        t->cb(subCxt);
    }

failed:
    freeReplyObject(reply);
    redisFree(c);
    free(args);
    return NULL;
}


// 发送sub或psub请求, 成功返回redisContext *，失败返回NULL
redisContext *sendSubRequest(const char *subCmd, int redisDbId, const char *key)
{
    struct timeval tv;
    tv.tv_sec = CONN_TIMEOUT_SEC;
    tv.tv_usec = CONN_TIMEOUT_USEC;
    redisContext *c = redisConnectWithTimeout(SERV_IP, SERV_PORT, tv);
    if (c == NULL || c->err) {
        if (c != NULL) {
            HiredisClientLog(LOG_ERR, "redisConnect failed, redisContext is not NULL!\n");
            redisFree(c);
        } else {
            HiredisClientLog(LOG_ERR, "redisConnect failed, redisContext is NULL!\n");
        }
        return NULL;
    }
    
    redisReply *r = (redisReply *)redisCommand(c, "%s __keyspace@%d__:%s", subCmd, redisDbId, key);
    if (r == NULL) {
        HiredisClientLog(LOG_ERR, "%s __keyspace@%d__:%s failed, reply is nil\n", subCmd, redisDbId, key);
        redisFree(c);
        return NULL;
    }
    freeReplyObject(r);
    return c;
}


static int createSubThread(redisContext *c, hiredisSubCallback cb, hiredisSubContext *cxt)
{
    pthread_t tid;
    SubThreadArgs *args = (SubThreadArgs *)malloc(sizeof(*args));
    args->c = c;
    args->cb = cb;
    args->cxt = cxt;
    int ret = pthread_create(&tid, NULL, SubThread, args);
    if (ret != 0) {
        HiredisClientLog(LOG_ERR, "createSubThread failed, pthread_create ret: %d\n", ret);
        free(args);
        redisFree(c);
        return HIREDIS_SUB_PTHREAD_CREATE_FAILED;
    }
    return HIREDIS_SUB_OK;
}

// Subscribe
static int _Subscribe(const char *key, hiredisSubCallback cb, hiredisSubContext *cxt)
{
    cxt->subType = HIREDIS_SUBSCRIBE;
    strcpy(cxt->channel, key);

    redisContext *c = sendSubRequest("SUBSCRIBE", SUB_DEFAULT_DBID, key);
    if (c == NULL) {
        return HIREDIS_SUB_CONNECT_ERR;
    }
    return createSubThread(c, cb, cxt);
}


// Psubscribe
static int _Psubscribe(const char *key, hiredisSubCallback cb, hiredisSubContext *cxt)
{
    cxt->subType = HIREDIS_PSUBSCRIBE;
    strcpy(cxt->pattern, key);

    redisContext *c = sendSubRequest("PSUBSCRIBE", SUB_DEFAULT_DBID, key);
    if (c == NULL) {
        return HIREDIS_SUB_CONNECT_ERR;
    }
    return createSubThread(c, cb, cxt);
}


// 订阅频道 "__keyspace@0:__key"; 发生set, del事件时, 触发自定义回调cb(cxt)
int HiredisSubscribe(const char *key, hiredisSubCallback cb, hiredisSubContext *cxt)
{
    if (cxt == NULL) {
        return HIREDIS_SUB_CONTEXT_IS_NIL;
    }
    if (cb == NULL) {
        return HIREDIS_SUB_CALLBACK_IS_NIL;
    }
    return _Subscribe(key, cb, cxt);
}


// 订阅模式 "__keyspace@0:__key"; 发生set, del事件时, 触发自定义回调cb(cxt)
int HiredisPsubscribe(const char *key, hiredisSubCallback cb, hiredisSubContext *cxt)
{
    if (cxt == NULL) {
        return HIREDIS_SUB_CONTEXT_IS_NIL;
    }
    if (cb == NULL) {
        return HIREDIS_SUB_CALLBACK_IS_NIL;
    }
    return _Psubscribe(key, cb, cxt);
}
