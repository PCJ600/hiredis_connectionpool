#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <hiredis/hiredis.h>
#include "hiredis_api.h"
#include "hiredis_log.h"
#include "hiredis_sub.h"

#define SERV_IP "127.0.0.1"
#define SERV_PORT 6379
#define NUM_READER 100
#define NUM_WRITER 100
#define NUM_RDWR   10

static const int OK = 0;
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
    printf("%s OK, cost: %ld s\n", __func__, t2 - t1);
}

void *PoolReader(void *args) {
    char key[16];
    char value[16];
    int ret;
    for (int i = 0; i < NUM_RDWR; ++i) {
        sprintf(key, "hello%d", i);
        ret = HiredisGetString(key, value);
        printf("[GET] key: %s, value: %s, ret: %d\n", key, value, ret);
        assert(ret == 0);
    }
    return NULL;
}

void *PoolWriter(void *args) {
    char key[16];
    char value[16];
    int ret;
    for (int i = 0; i < NUM_RDWR; ++i) {
        sprintf(key, "hello%d", i);
        sprintf(value, "world%d", i);
        ret = HiredisSetString(key, value);
        printf("[SET] key: %s, value: %s, ret: %d\n", key, value, ret);
        assert(ret == 0);
    }
    return NULL;
}

void testcase_connpool()
{
    time_t t1 = time(NULL);

    pthread_t wid[NUM_WRITER];
    pthread_t rid[NUM_READER];
    for (int i = 0; i < NUM_WRITER; ++i) {
        pthread_create(&wid[i], NULL, PoolWriter, NULL);
    }
    for (int i = 0; i < NUM_WRITER; ++i) {
        pthread_join(wid[i], NULL);
    }
    for (int i = 0; i < NUM_READER; ++i) {
        pthread_create(&rid[i], NULL, PoolReader, NULL);
    }
    for (int i = 0; i < NUM_READER; ++i) {
        pthread_join(rid[i], NULL);
    }

    time_t t2 = time(NULL);
    printf("%s OK, cost: %ld s\n", __func__, t2 - t1);
}

void testcase_connpool_set_server_timeout()
{
    time_t t1 = time(NULL);

    assert(HiredisGenericCommand("CONFIG SET timeout %d", 1) == 0); // timeout: 1 s
    sleep(5);  // server timeout

    pthread_t wid[NUM_WRITER];
    pthread_t rid[NUM_READER];
    for (int i = 0; i < NUM_WRITER; ++i) {
        pthread_create(&wid[i], NULL, PoolWriter, NULL);
    }
    for (int i = 0; i < NUM_WRITER; ++i) {
        pthread_join(wid[i], NULL);
    }

    for (int i = 0; i < NUM_READER; ++i) {
        pthread_create(&rid[i], NULL, PoolReader, NULL);
    }
    for (int i = 0; i < NUM_READER; ++i) {
        pthread_join(rid[i], NULL);
    }

    assert(HiredisGenericCommand("CONFIG SET timeout %d", 0) == 0);
    time_t t2 = time(NULL);
    printf("%s OK, cost: %ld s\n", __func__, t2 - t1);
}

void testcase_getset_int32()
{
    // set intkey 12345
    uint32_t val = 0;
    int ret = HiredisSetUint32("intkey", 12345);
    assert(ret == 0);
    ret = HiredisGetUint32("intkey", &val);
    assert(ret == 0 && val == 12345);

    // set key 2147483649
    ret = HiredisSetUint32("intkey", UINT_MAX);
    assert(ret == 0);
    ret = HiredisGetUint32("intkey", &val);
    assert(ret == 0 && val == UINT_MAX);

    // set intkey -2147483648
    int ival = 0;
    ret = HiredisSetInt32("intkey", INT_MIN);
    assert(ret == 0);
    ret = HiredisGetInt32("intkey", &ival);
    assert(ret == 0 && ival == INT_MIN);
    
    // set intkey 2147483647
    ret = HiredisSetInt32("intkey", INT_MAX);
    assert(ret == 0);
    ret = HiredisGetInt32("intkey", &ival);
    assert(ret == 0 && ival == INT_MAX);

    printf("%s OK\n", __func__);
}

void testcase_getset_int64()
{
    // set int64key 12345
    uint64_t val = 0;
    int ret = HiredisSetUint64("int64key", 12345);
    assert(ret == 0);
    ret = HiredisGetUint64("int64key", &val);
    assert(ret == 0 && val == 12345);

    // set int64key ULONG_MAX
    ret = HiredisSetUint64("int64key", ULLONG_MAX);
    assert(ret == 0);
    ret = HiredisGetUint64("int64key", &val);
    assert(ret == 0 && val == ULLONG_MAX);

    // set int64key LONG_MIN
    int64_t ival = 0;
    ret = HiredisSetInt64("int64key", LLONG_MIN);
    assert(ret == 0);
    ret = HiredisGetInt64("int64key", &ival);
    assert(ret == 0 && ival == LLONG_MIN);
    
    // set int64key LONG_MAX
    ret = HiredisSetInt64("int64key", LLONG_MAX);
    assert(ret == 0);
    ret = HiredisGetInt64("int64key", &ival);
    assert(ret == 0 && ival == LLONG_MAX);

    printf("%s OK\n", __func__);
}


void testcase_pool()
{
    testcase_connpool();
    testcase_connpool_set_server_timeout();
    testcase_getset_int32();
    testcase_getset_int64();
}

void testcase_log()
{
    HiredisClientLog(LOG_WARNING, "hello world!\n");
    HiredisClientLog(LOG_WARNING, "cost: %d\n", 1);
    HiredisClientLog(LOG_WARNING, "string: %s\n", "abc");
    printf("%s OK\n", __func__);
}


// testcase for subscribe/psubscribe
static int g_counter = 0;
struct testFuncCxt {
    int (*getSum)(int a, int b);
    int a;
    int b;
};

int testFunc(int a, int b)
{
    int sum = a + b;
    g_counter += sum;
    printf("a: %d, b: %d, sum: %d, g_counter: %d\n", a, b, sum, g_counter);
    return sum;
}

void testCb(hiredisSubContext *cxt)
{
    struct testFuncCxt *s = (struct testFuncCxt *)(cxt->args);
    s->getSum(s->a, s->b);
}

void testcase_sub()
{
    g_counter = 0;
    hiredisSubContext *cxt = (hiredisSubContext *)malloc(sizeof(*cxt));
    struct testFuncCxt tfc = {testFunc, 1, 2};
    cxt->args = (void *)&tfc;

    assert(HiredisSubscribe("hello", testCb, cxt) == 0);
    sleep(3); // ensure subscribe finish before dbset!

    const int NUM_REPEAT = 10;
    for (int i = 0; i < NUM_REPEAT; ++i) {
        assert(HiredisSetString("hello", "world") == 0);
    }
    sleep(3); // ensure callback finish before assert!
    assert(g_counter == ((1 + 2) * NUM_REPEAT));
    printf("%s OK\n", __func__);
}

void testcase_psub()
{
    g_counter = 0;
    hiredisSubContext *cxt = (hiredisSubContext *)malloc(sizeof(*cxt));
    struct testFuncCxt tfc = {testFunc, 1, 2};
    cxt->args = (void *)&tfc;

    assert(HiredisPsubscribe("pat*", testCb, cxt) == 0);
    sleep(3); // ensure subscribe finish before dbset!

    const int NUM_REPEAT = 10;
    for (int i = 0; i < NUM_REPEAT; ++i) {
        assert(HiredisSetString("pattern", "123") == 0);
    }
    sleep(3); // ensure callback finish before assert!
    assert(g_counter == ((1 + 2) * NUM_REPEAT));
    printf("%s OK\n", __func__);
}

int main() {
    testcase_single_connection();
    testcase_log();
    testcase_pool();
    testcase_sub();
    testcase_psub();
    return 0;
}
