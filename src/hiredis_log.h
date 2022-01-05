#ifndef HIREDIS_LOG_H
#define HIREDIS_LOG_H
#include <syslog.h>

void HiredisClientLog(int level, const char *fmt, ...);

#endif
