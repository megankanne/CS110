#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "debug.h"


char debugFlags[256];

void
Debug_SetFlag(char ch, int val)
{
  debugFlags[(unsigned char)ch] = val;
}

/*
 * Get the number of microseconds since the epoch (midnight, January 1, 1970).
 */
int64_t
Debug_GetTimeInMicrosecs(void)
{
  struct timeval curtime;
  int err = gettimeofday(&curtime, NULL);

  if (err < 0) {
    fprintf(stderr, "Can't read time of day\n");
    return 0;
  }

  return  (curtime.tv_sec * (int64_t) 1000000) + curtime.tv_usec;
}
