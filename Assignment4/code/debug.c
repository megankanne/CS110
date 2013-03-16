#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "debug.h"


char debugFlags[256];

void
Debug_SetFlag(char ch, int val)
{
  debugFlags[(int)ch] = val;
}

/*
 * get micro second of current time
 */
int64_t
Debug_GetTimeInMicrosecs(void)
{
  struct timeval curtime;
  int err = gettimeofday(&curtime, NULL);
  if (err < 0) {
    fprintf(stderr, "Can't read time of day!\n");
    return 0;
  }
  return  ((curtime.tv_sec * (int64_t) 1000000) + curtime.tv_usec);
}
