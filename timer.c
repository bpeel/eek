#include "config.h"

#include <stdlib.h>
#include <sys/time.h>

#include "eek.h"
#include "timer.h"

struct timeval timer_start;

void
timer_init (void)
{
  gettimeofday (&timer_start, NULL);
}

unsigned int
timer_ticks (void)
{
  struct timeval now;

  gettimeofday (&now, NULL);

  return (now.tv_sec - timer_start.tv_sec) * 1000
    + (now.tv_usec - timer_start.tv_usec) / 1000;
}
