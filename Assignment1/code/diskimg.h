#ifndef DISKIMG_H
#define DISKIMG_H

#include <stdint.h>

/* Size of a disk sector in bytes. */
#define DISKIMG_SECTOR_SIZE 512

int diskimg_open(char *pathname, int readOnly);
int diskimg_getsize(int fd); 
int diskimg_readsector(int fd, int sectorNum, void *buf); 
int diskimg_writesector(int fd, int sectorNum, void *buf); 
int diskimg_close(int fd);

#endif /* diskimg.h */
