#include "hiredis_sub.h"
#include "hiredis_log.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis/hiredis.h>
#include <unistd.h>

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
            freeReplyObject(reply);
            redisFree(c);
            free(args);
            return NULL;
        }
        
        char *eventType = ((redisReply *)reply)->element[2]->str;   // reply->element: [message, channel, eventType]
        subCxt->event = _GetEventType(eventType);
        t->cb(subCxt);
        freeReplyObject(reply);
    }
    redisFree(c);
    free(args);
    return NULL;
}

// Subscribe
static int _Subscribe(const char *key, hiredisSubCallback cb, hiredisSubContext *cxt)
{
    cxt->subType = HIREDIS_SUBSCRIBE;
    strcpy(cxt->channel, key);

    // 1、连接
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
        return HIREDIS_SUB_CONNECT_ERR;
    }
    
    redisReply *r = (redisReply *)redisCommand(c, "SUBSCRIBE __keyspace@0__:%s", key);
    if (r == NULL) {
        HiredisClientLog(LOG_ERR, "SUBSCRIBE __keyspace@0__:%s failed, reply is nil\n", key);
        redisFree(c);
        return HIREDIS_SUB_REPLY_IS_NIL;
    }
    freeReplyObject(r);
   
    // 2、创建后台线程，死循环，每读取一个订阅消息触发一次回调
    pthread_t tid;
    SubThreadArgs *args = (SubThreadArgs *)malloc(sizeof(*args));
    args->c = c;
    args->cb = cb;
    args->cxt = cxt;
    int ret = pthread_create(&tid, NULL, SubThread, args);
    if (ret != 0) {
        HiredisClientLog(LOG_ERR, "SUBSCRIBE __keyspace@0__:%s failed, pthread_create ret: %d\n", key, ret);
        return HIREDIS_SUB_PTHREAD_CREATE_FAILED;
    }
    return HIREDIS_SUB_OK;
}


// Psubscribe
static int _Psubscribe(const char *key, hiredisSubCallback cb, hiredisSubContext *cxt)
{
    cxt->subType = HIREDIS_PSUBSCRIBE;
    strcpy(cxt->pattern, key);
    return HIREDIS_SUB_OK;
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
