#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "hiredis.h"
#include <pthread.h>

#define SERV_IP "127.0.0.1"
#define SERV_PORT 6379
#define NUM_READER 500
#define NUM_WRITER 500
#define NUM_RDWR   10

static const int OK = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int DBGetString(const char *key, char *value) {
    struct timeval tv; 
    tv.tv_sec = 5; tv.tv_usec = 0;
    redisContext *c = redisConnectWithTimeout(SERV_IP, SERV_PORT, tv);
    assert(c != NULL && !c->err);

    char cmd[256];
    sprintf(cmd, "GET %s", key);
    redisReply *reply = (redisReply *)redisCommand(c, cmd);
    assert(reply->str != NULL);
    strcpy(value, reply->str);
    freeReplyObject(reply);
    redisFree(c);
    return OK;
}

int DBSetString(const char *key, const char *value) {
    struct timeval tv; 
    tv.tv_sec = 5; tv.tv_usec = 0;
    redisContext *c = redisConnectWithTimeout(SERV_IP, SERV_PORT, tv);
    assert(c != NULL && !c->err);

    char cmd[256];
    sprintf(cmd, "SET %s %s", key, value);
    redisReply *reply = (redisReply *)redisCommand(c, cmd);
    freeReplyObject(reply);
    redisFree(c);
    return OK;
}

void *Reader(void *args) {
    char key[16];
    char value[16];
    for (int i = 0; i < NUM_RDWR; ++i) {
        sprintf(key, "hello%d", i);
        sprintf(value, "world%d", i);
        DBGetString(key, value);
        printf("key: %s, value: %s\n", key, value);
    }
    return NULL;
}

void *Writer(void *args) {
    char key[16];
    char value[16];
    for (int i = 0; i < NUM_RDWR; ++i) {
        sprintf(key, "hello%d", i);
        sprintf(value, "world%d", i);
        DBSetString(key, value);
    }
    return NULL;
}

void testcase() {
    time_t t1 = time(NULL);
    pthread_t wid[NUM_WRITER];
    pthread_t rid[NUM_READER];

    for (int i = 0; i < NUM_WRITER; ++i) {
        pthread_create(&wid[i], NULL, Writer, NULL);
    }
    for (int i = 0; i < NUM_READER; ++i) {
        pthread_create(&rid[i], NULL, Reader, NULL);
    }

    for (int i = 0; i < NUM_WRITER; ++i) {
        pthread_join(wid[i], NULL);
    }
    for (int i = 0; i < NUM_READER; ++i) {
        pthread_join(rid[i], NULL);
    }
    time_t t2 = time(NULL);
    printf("%ld s\n", t2 - t1);
}


int main() {
    testcase();
    return 0;
}
