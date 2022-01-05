#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <hiredis/hiredis.h>
#include "hiredis_connpool.h"
#include "hiredis_api.h"

#define SERV_IP "127.0.0.1"
#define SERV_PORT 6379
#define NUM_READER 100
#define NUM_WRITER 100
#define NUM_RDWR   10

static const int OK = 0;
static const int MAX_NUM_CONNECTIONS_PER_CONNPOOL = 10;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int DBShortConnGetString(redisContext *c, const char *key, char *value) {
    char cmd[256];
    sprintf(cmd, "GET %s", key);
    pthread_mutex_lock(&mutex);
    redisReply *reply = (redisReply *)redisCommand(c, cmd);
    pthread_mutex_unlock(&mutex);
    
    if (reply->type == REDIS_REPLY_NIL) {
        value[0] = '\0';
    } else {
        assert(reply->type == REDIS_REPLY_STRING);
        assert(reply->str != NULL);
        strcpy(value, reply->str);
    }
    freeReplyObject(reply);
    return OK;
}

int DBShortConnSetString(redisContext *c, const char *key, const char *value) {
    char cmd[256];
    sprintf(cmd, "SET %s %s", key, value);
    pthread_mutex_lock(&mutex);
    redisReply *reply = (redisReply *)redisCommand(c, cmd);
    pthread_mutex_unlock(&mutex);
    assert(strcmp(reply->str, "OK") == 0);
    freeReplyObject(reply);
    return OK;
}

void *Reader(void *args) {
    redisContext *c = (redisContext *)args;
    char key[16];
    char value[16];
    for (int i = 0; i < NUM_RDWR; ++i) {
        sprintf(key, "hello%d", i);
        sprintf(value, "world%d", i);
        DBShortConnGetString(c, key, value);
        printf("key: %s, value: %s\n", key, value);
    }
    return NULL;
}

void *Writer(void *args) {
    redisContext *c = (redisContext *)args;
    char key[16];
    char value[16];
    for (int i = 0; i < NUM_RDWR; ++i) {
        sprintf(key, "hello%d", i);
        sprintf(value, "world%d", i);
        DBShortConnSetString(c, key, value);
        printf("key: %s, value: %s\n", key, value);
    }
    return NULL;
}

void testcase_single_connection() {
    time_t t1 = time(NULL);
    pthread_t wid[NUM_WRITER];
    pthread_t rid[NUM_READER];

    struct timeval tv; 
    tv.tv_sec = 5; tv.tv_usec = 0;
    redisContext *c = redisConnectWithTimeout(SERV_IP, SERV_PORT, tv);
    assert(c != NULL && !c->err);

    for (int i = 0; i < NUM_WRITER; ++i) {
        pthread_create(&wid[i], NULL, Writer, c);
    }
    for (int i = 0; i < NUM_WRITER; ++i) {
        pthread_join(wid[i], NULL);
    }

    for (int i = 0; i < NUM_READER; ++i) {
        pthread_create(&rid[i], NULL, Reader, c);
    }
    for (int i = 0; i < NUM_READER; ++i) {
        pthread_join(rid[i], NULL);
    }
    redisFree(c);
    time_t t2 = time(NULL);
    printf("%ld s\n", t2 - t1);
}

void *PoolReader(void *args) {
    DBConnPool *p = (DBConnPool *)args;
    char key[16];
    char value[16];
    int ret;
    for (int i = 0; i < NUM_RDWR; ++i) {
        sprintf(key, "hello%d", i);
        ret = DBGetString(p, key, value);
        printf("[GET] key: %s, value: %s, ret: %d\n", key, value, ret);
        assert(ret == 0);
    }
    return NULL;
}

void *PoolWriter(void *args) {
    DBConnPool *p = (DBConnPool *)args;
    char key[16];
    char value[16];
    int ret;
    for (int i = 0; i < NUM_RDWR; ++i) {
        sprintf(key, "hello%d", i);
        sprintf(value, "world%d", i);
        ret = DBSetString(p, key, value);
        printf("[SET] key: %s, value: %s, ret: %d\n", key, value, ret);
        assert(ret == 0);
    }
    return NULL;
}

void testcase_connpool()
{
    time_t t1 = time(NULL);

    DBConnPool *p = DBConnPoolCreate(MAX_NUM_CONNECTIONS_PER_CONNPOOL);
    assert(p != NULL);

    pthread_t wid[NUM_WRITER];
    pthread_t rid[NUM_READER];
    for (int i = 0; i < NUM_WRITER; ++i) {
        pthread_create(&wid[i], NULL, PoolWriter, p);
    }
    for (int i = 0; i < NUM_WRITER; ++i) {
        pthread_join(wid[i], NULL);
    }
    for (int i = 0; i < NUM_READER; ++i) {
        pthread_create(&rid[i], NULL, PoolReader, p);
    }
    for (int i = 0; i < NUM_READER; ++i) {
        pthread_join(rid[i], NULL);
    }

    DBConnPoolDestroy(p);

    time_t t2 = time(NULL);
    printf("%ld s\n", t2 - t1);

}

void testcase_connpool_set_server_timeout()
{
    time_t t1 = time(NULL);

    DBConnPool *p = DBConnPoolCreate(MAX_NUM_CONNECTIONS_PER_CONNPOOL);
    assert(p != NULL);
    assert(DBGenericCommand(p, "CONFIG SET timeout %d", 1) == 0); // timeout: 1 s
    sleep(5);  // server timeout

    pthread_t wid[NUM_WRITER];
    pthread_t rid[NUM_READER];
    for (int i = 0; i < NUM_WRITER; ++i) {
        pthread_create(&wid[i], NULL, PoolWriter, p);
    }
    for (int i = 0; i < NUM_WRITER; ++i) {
        pthread_join(wid[i], NULL);
    }

    for (int i = 0; i < NUM_READER; ++i) {
        pthread_create(&rid[i], NULL, PoolReader, p);
    }
    for (int i = 0; i < NUM_READER; ++i) {
        pthread_join(rid[i], NULL);
    }

    assert(DBGenericCommand(p, "CONFIG SET timeout %d", 0) == 0);
    DBConnPoolDestroy(p);
    time_t t2 = time(NULL);
    printf("%ld s\n", t2 - t1);
}

void testcase_getset_int32()
{
    DBConnPool *p = DBConnPoolCreate(MAX_NUM_CONNECTIONS_PER_CONNPOOL);
    assert(p != NULL);

    // set intkey 12345
    uint32_t val = 0;
    int ret = DBSetUint32(p, "intkey", 12345);
    assert(ret == 0);
    ret = DBGetUint32(p, "intkey", &val);
    assert(ret == 0 && val == 12345);

    // set key 2147483649
    ret = DBSetUint32(p, "intkey", UINT_MAX);
    assert(ret == 0);
    ret = DBGetUint32(p, "intkey", &val);
    assert(ret == 0 && val == UINT_MAX);

    // set intkey -2147483648
    int ival = 0;
    ret = DBSetInt32(p, "intkey", INT_MIN);
    assert(ret == 0);
    ret = DBGetInt32(p, "intkey", &ival);
    assert(ret == 0 && ival == INT_MIN);
    
    // set intkey 2147483647
    ret = DBSetInt32(p, "intkey", INT_MAX);
    assert(ret == 0);
    ret = DBGetInt32(p, "intkey", &ival);
    assert(ret == 0 && ival == INT_MAX);

    DBConnPoolDestroy(p);
    printf("%s OK\n", __func__);
}

void testcase_getset_int64()
{
    DBConnPool *p = DBConnPoolCreate(MAX_NUM_CONNECTIONS_PER_CONNPOOL);
    assert(p != NULL);

    // set int64key 12345
    uint64_t val = 0;
    int ret = DBSetUint64(p, "int64key", 12345);
    assert(ret == 0);
    ret = DBGetUint64(p, "int64key", &val);
    assert(ret == 0 && val == 12345);

    // set int64key ULONG_MAX
    ret = DBSetUint64(p, "int64key", ULLONG_MAX);
    assert(ret == 0);
    ret = DBGetUint64(p, "int64key", &val);
    assert(ret == 0 && val == ULLONG_MAX);

    // set int64key LONG_MIN
    int64_t ival = 0;
    ret = DBSetInt64(p, "int64key", LLONG_MIN);
    assert(ret == 0);
    ret = DBGetInt64(p, "int64key", &ival);
    assert(ret == 0 && ival == LLONG_MIN);
    
    // set int64key LONG_MAX
    ret = DBSetInt64(p, "int64key", LLONG_MAX);
    assert(ret == 0);
    ret = DBGetInt64(p, "int64key", &ival);
    assert(ret == 0 && ival == LLONG_MAX);

    DBConnPoolDestroy(p);
    printf("%s OK\n", __func__);
}


void testCb(redisAsyncContext *c, void *r, void *privdata)
{
    redisReply *reply = (redisReply *)r;
    if (reply == NULL) {
        assert("pubsub callback failed, reply is null");
        return;
    }
    if (reply->type == REDIS_REPLY_ARRAY) {
        if (strcmp(reply->element[0]->str, "psubscribe") == 0) {
            return;
        }

        for (int i = 0; i < reply->elements; ++i) {
            printf("%s\n ", reply->element[i]->str);
        }
    }
    return;
}

void testcase_pubsub()
{
    const int PUBSUB_NUM = 100;
    char buf[256];
    for (int i = 0; i < PUBSUB_NUM; ++i) {
        sprintf(buf, "hello%d", i);
        DBPSubscribeKeyspaceEvent(buf, testCb, NULL);
    }

    DBConnPool *p = DBConnPoolCreate(MAX_NUM_CONNECTIONS_PER_CONNPOOL);
    for (int i = 0; i < PUBSUB_NUM; ++i) {
        sprintf(buf, "hello%d", i);
        DBSetString(p, buf, "1");
    }

    DBConnPoolDestroy(p);
    printf("%s OK, press Ctrl+C \n", __func__);
    pause();
}

void testcase_pool()
{
    testcase_connpool();
    testcase_connpool_set_server_timeout();
    testcase_getset_int32();
    testcase_getset_int64();
    testcase_pubsub();
}

void testcase_pool_hide()
{
    const int PUBSUB_NUM = 100;
    char buf[256];
    for (int i = 0; i < PUBSUB_NUM; ++i) {
        sprintf(buf, "hello%d", i);
        DBPSubscribeKeyspaceEvent(buf, testCb, NULL);
    }
    for (int i = 0; i < PUBSUB_NUM; ++i) {
        sprintf(buf, "hello%d", i);
        HiredisSetString(buf, "1");
    }
    printf("%s OK, press Ctrl+C \n", __func__);
    pause();
}

int main() {
    testcase_single_connection();
    testcase_pool();
    return 0;
}
