#ifndef TCC_STUB_TIME_H
#define TCC_STUB_TIME_H
#include <__tcc_stub_common.h>

typedef long time_t;
typedef int clockid_t;

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

#define CLOCK_MONOTONIC 6

time_t time(time_t *tloc);
int clock_gettime(clockid_t clock_id, struct timespec *tp);
struct tm *localtime(const time_t *timep);

#endif
