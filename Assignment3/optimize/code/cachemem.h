#ifndef _CACHEMEM_H
#define _CACHEMEM_H

#include "proj1/unixfilesystem.h"


/*
 * The main export of the cachemem module is the memory for the cache
 * pointed to by the following global variables:
 * cacheMemSizeInKB - The size of the cache memory in kiloytes. 
 * cacheMemPtr      - Starting address of the cache memory. 
 */ 
extern int cacheMemSizeInKB;
extern void *cacheMemPtr;

#define CACHEMEM_MAX_SIZE (64*1024*1024)

int CacheMem_Init(int sizeInKB);
int putSectorInCache(struct unixfilesystem *fs, int sectorNum);
int totalCacheSize();
int getSectorFromCache(int sectorNum, void *buf);

#endif /* _CACHEMEM_H */
