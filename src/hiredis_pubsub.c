#include "hiredis_api.h"
#include <string.h>
#include <signal.h>
#include <adapters/libevent.h>

void connectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
        printf("%s Error: %s\n", __func__, c->errstr);
    }
}
 
void disconnectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
        printf("%s Error: %s\n", __func__, c->errstr);
    }
}

typedef struct PsubKeyspaceEventContext {
    char pattern[256];
    redisCallbackFn *fn;
    void *args;
} PsubKeyspaceEventContext;

// 订阅数据库键空间发生的事件 (set, del, expire ...)
void *PsubscribeKeyspaceEventThread(void *args)
{
    pthread_detach(pthread_self());

    signal(SIGPIPE, SIG_IGN);
    struct event_base *base = event_base_new();
    redisAsyncContext *c = redisAsyncConnect(SERV_IP, SERV_PORT);//新建异步连接
    if (c->err) {
        redisAsyncFree(c);
        return NULL;
    }
 
    PsubKeyspaceEventContext *cxt = (PsubKeyspaceEventContext *)args;
    redisLibeventAttach(c,base);
    redisAsyncSetConnectCallback(c,connectCallback);
    redisAsyncSetDisconnectCallback(c,disconnectCallback);
    redisAsyncCommand(c, cxt->fn, cxt->args, "PSUBSCRIBE __keyspace@0__:%s", cxt->pattern);
    event_base_dispatch(base);
    return NULL;
}

// pubsub API
int DBPSubscribeKeyspaceEvent(const char *pattern, redisCallbackFn *fn, void *args)
{
    pthread_t t;
    PsubKeyspaceEventContext *cxt = (PsubKeyspaceEventContext *)malloc(sizeof(*cxt));
    if (cxt == NULL) {
        return HIREDIS_MALLOC_FAILED;
    }
    strcpy(cxt->pattern, pattern);
    cxt->fn = fn;
    cxt->args = args;

    int ret = pthread_create(&t, NULL, PsubscribeKeyspaceEventThread, cxt);
    if (ret != 0) {
        return HIREDIS_PTHREAD_CREATE_FAILED;
    }
    return HIREDIS_OK;
}
