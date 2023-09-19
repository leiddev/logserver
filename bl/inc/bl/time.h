#ifndef _BL_TIME_H_
#define _BL_TIME_H_

#ifdef _WINDOWS
#include <time.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "exp.h"

namespace bl {

typedef enum  {
  TIME_FORMAT_DEFAULT   = 0,
  TIME_FORMAT_1,
  TIME_FORMAT_2,
} TIME_FORMAT_T;

BL_API void CALL timenow(char *t, TIME_FORMAT_T format=TIME_FORMAT_DEFAULT);

BL_API int CALL gettimeofday(struct timeval *tp, void *tzp);

BL_API int timestamp(void);
BL_API double timestamp_ms(void);
BL_API unsigned long long system_tick_ms(void);

BL_API long long ts_to_ns(struct timespec tm);
BL_API struct timespec ns_to_ts(long long ns);

BL_API void CALL sleep(int ms);

BL_API int CALL set_timestamp(long int sec, long int usec);
}  // namespace bo


#define TIC                    \
  struct timeval time1, time2; \
  bl::gettimeofday(&time1, NULL);

#define TOC                                                   \
  bl::gettimeofday(&time2, NULL);                             \
  double elapsed_time =                                       \
      (double)(time2.tv_sec - time1.tv_sec) * 1000 +          \
      (double)((std::uint64_t)time2.tv_usec - time1.tv_usec) / 1000; \
  LOG_DEBUG("elapsed time %.2f ms \n", elapsed_time);

#endif
