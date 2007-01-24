#include "config.h"

#include <stdlib.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <time.h>
#endif

#include "eek.h"
#include "timer.h"

#ifdef WIN32
static DWORD timer_start;
#else
static struct timeval timer_start;
#endif

void
timer_init (void)
{
#ifdef WIN32
  timer_start = GetTickCount ();
#else
  gettimeofday (&timer_start, NULL);
#endif
}

unsigned int
timer_ticks (void)
{
#ifdef WIN32
  return GetTickCount () - timer_start;
#else
  struct timeval now;

  gettimeofday (&now, NULL);

  return (now.tv_sec - timer_start.tv_sec) * 1000
    + (now.tv_usec - timer_start.tv_usec) / 1000;
#endif
}

void
timer_sleep (int sleep_time)
{
#ifdef WIN32
  Sleep (sleep_time);
#else
  struct timespec t, r;
  t.tv_sec = sleep_time / 1000;
  t.tv_nsec = sleep_time * 1000;
  nanosleep (&t, &r);
#endif
}
