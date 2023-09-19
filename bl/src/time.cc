/*
* lujy 2021.6
*/
#include "bl/time.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

namespace bl{
int gettimeofday(struct timeval *tv, void *tz) {
#ifdef WIN32
  time_t clock;
  struct tm tm;
  SYSTEMTIME wtm;
  GetLocalTime(&wtm);
  tm.tm_year = wtm.wYear - 1900;
  tm.tm_mon = wtm.wMonth - 1;
  tm.tm_mday = wtm.wDay;
  tm.tm_hour = wtm.wHour;
  tm.tm_min = wtm.wMinute;
  tm.tm_sec = wtm.wSecond;
  tm.tm_isdst = -1;
  clock = mktime(&tm);
  tv->tv_sec = (long)clock;
  tv->tv_usec = wtm.wMilliseconds * 1000;
#else
  ::gettimeofday(tv, NULL);
#endif
  return 0;
}

static const char *weekday_chn[] = { "日", "一", "二", "三", "四", "五", "六" };
static const char *weekday_eng[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

void timenow(char *t, TIME_FORMAT_T format) {
  struct timeval tv;
  struct tm *p = NULL;
#ifdef WIN32
  struct tm tm;
  SYSTEMTIME wtm;
#endif

  if (!t)
    return;
  bl::gettimeofday(&tv, NULL);

#ifdef WIN32
  GetLocalTime(&wtm);
  tm.tm_year = wtm.wYear - 1900;
  tm.tm_mon = wtm.wMonth - 1;
  tm.tm_mday = wtm.wDay;
  tm.tm_hour = wtm.wHour;
  tm.tm_min = wtm.wMinute;
  tm.tm_sec = wtm.wSecond;
  tm.tm_isdst = -1;
  p = &tm;
#else
  p = localtime(&tv.tv_sec);
#endif
  if (p) {
    switch(format) {
      case TIME_FORMAT_1:
        sprintf(t, "%04d年%02d月%02d日 星期%s %02d:%02d:%02d", 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday,
                weekday_chn[p->tm_wday], p->tm_hour, p->tm_min, p->tm_sec);
        break;
      case TIME_FORMAT_2:
        sprintf(t, "%04d-%02d-%02d %s %02d:%02d:%02d", 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday,
                weekday_eng[p->tm_wday], p->tm_hour, p->tm_min, p->tm_sec);
        break;
      default:
        sprintf(t, "[%04d-%02d-%02d_%02d:%02d:%02d.%03d]", 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, p->tm_hour,
                p->tm_min, p->tm_sec, int(tv.tv_usec / 1000));
    }
  }
}

int timestamp(void) {
    struct timeval tv;
    bl::gettimeofday(&tv, NULL);

    return tv.tv_sec;
}

double timestamp_ms(void) {
  struct timeval tv;
  bl::gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}
unsigned long long system_tick_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    unsigned long long ullTime = ts.tv_sec;
    ullTime = ullTime * 1000 + ts.tv_nsec / 1000000;
    return ullTime;
}

long long ts_to_ns(struct timespec ts) {
  return (long long)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

struct timespec ns_to_ts(long long ns) {
  struct timespec ts;
  ts.tv_sec = ns / 1000000000;
  ts.tv_nsec = ns - ((long long)ts.tv_sec * 1000000000);
  return ts;
}

void sleep(int ms) {
#ifdef WIN32
  ::Sleep(ms);
#else
  usleep(ms * 1000);
#endif
}

int set_timestamp(long int sec, long int usec) {
    struct timeval tv;

    tv.tv_sec = sec;
    tv.tv_usec = usec;
    
    if (settimeofday(&tv, NULL) < 0) {
        return -1;
    }

    system("hwclock -w -u");
    
    return 0;
}

}
