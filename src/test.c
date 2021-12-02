#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <hiredis/hiredis.h>
#include "hiredis_connpool.h"
#include "hiredis_api.h"

#define SERV_IP "127.0.0.1"
#define SERV_PORT 6379
#define NUM_READER 1000
#define NUM_WRITER 1000
#define NUM_RDWR   50

static const int OK = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int DBGetString(redisContext *c, const char *key, char *value) {
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

int DBSetString(redisContext *c, const char *key, const char *value) {
    char cmd[256];
    sprintf(cmd, "SET %s %s", key, value);
    pthread_mutex_lock(&mutex);
    redisReply *reply = (redisReply *)redisCommand(c, cmd);
    assert(!strcmp(reply->str, "OK"));
    pthread_mutex_unlock(&mutex);
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
        DBGetString(c, key, value);
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
        DBSetString(c, key, value);
    }
    return NULL;
}

void testcase_with_single_connection() {
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
    for (int i = 0; i < NUM_READER; ++i) {
        pthread_create(&rid[i], NULL, Reader, c);
    }

    for (int i = 0; i < NUM_WRITER; ++i) {
        pthread_join(wid[i], NULL);
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
    for (int i = 0; i < NUM_RDWR; ++i) {
        sprintf(key, "hello%d", i);
        sprintf(value, "world%d", i);
        DBConnPoolGetString(p, key, value);
        printf("key: %s, value: %s\n", key, value);
    }
    return NULL;
}

void *PoolWriter(void *args) {
    DBConnPool *p = (DBConnPool *)args;
    char key[16];
    char value[16];
    for (int i = 0; i < NUM_RDWR; ++i) {
        sprintf(key, "hello%d", i);
        sprintf(value, "world%d", i);
        DBConnPoolSetString(p, key, value);
    }
    return NULL;
}

void testcase_with_connpool()
{
    time_t t1 = time(NULL);

    const int MAX_NUM_CONNECTIONS_PER_CONNPOOL = 30;
    DBConnPool *p = DBConnPoolCreate(MAX_NUM_CONNECTIONS_PER_CONNPOOL);
    assert(p != NULL);

    pthread_t wid[NUM_WRITER];
    pthread_t rid[NUM_READER];
    for (int i = 0; i < NUM_WRITER; ++i) {
        pthread_create(&wid[i], NULL, PoolWriter, p);
    }
    for (int i = 0; i < NUM_READER; ++i) {
        pthread_create(&rid[i], NULL, PoolReader, p);
    }
    for (int i = 0; i < NUM_WRITER; ++i) {
        pthread_join(wid[i], NULL);
    }
    for (int i = 0; i < NUM_READER; ++i) {
        pthread_join(rid[i], NULL);
    }

    DBConnPoolDestroy(p);

    time_t t2 = time(NULL);
    printf("%ld s\n", t2 - t1);

}

int main() {
    // testcase_with_single_connection();
    testcase_with_connpool();
    return 0;
}
