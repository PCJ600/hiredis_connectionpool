#include "hiredis_log.h"
#include <stdarg.h>
#include <stdio.h>

#define MAX_LOG_BUF 1024

static void HiredisClientLogRaw(int level, const char *buf)
{
    syslog(level, "%s", buf);
}

void HiredisClientLog(int level, const char *fmt, ...)
{
    va_list ap;
    char buf[MAX_LOG_BUF];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    HiredisClientLogRaw(level, buf);
}
