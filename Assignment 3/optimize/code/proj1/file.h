#ifndef _FILE_H
#define _FILE_H

#include "unixfilesystem.h"


int file_getblock(struct unixfilesystem *fs, int inumber, int blockNo, void *buf); 

#endif /* _FILE_H */
