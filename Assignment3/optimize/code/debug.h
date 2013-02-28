#ifndef _DEBUG_H
#define _DEBUG_H

#include <stdio.h>
#include <stdint.h>

extern char debugFlags[];

#define DPRINTF(_flag, _args) if (debugFlags[_flag]) printf _args;

void Debug_SetFlag(char ch, int val);

int64_t Debug_GetTimeInMicrosecs(void);

#endif /* _DEBUG_H */
