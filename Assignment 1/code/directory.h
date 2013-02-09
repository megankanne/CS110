#ifndef _DIRECTORY_H
#define _DIRECTORY_H

#include "unixfilesystem.h"

#include "direntv6.h"

int directory_findname(struct unixfilesystem *fs, const char *name,
                       int dirinumber, struct direntv6 *dirEnt);

#endif /* _DIECTORY_H */
