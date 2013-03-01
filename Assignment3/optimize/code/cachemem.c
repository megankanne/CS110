/*
 * cachemem.c  -  This module allocates the memory for caches. 
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <inttypes.h> // for PRIu64
#include <openssl/lhash.h>

//#if defined(__APPLE__)
  #define _LHASH LHASH
//#endif

#include <sys/mman.h>

#include "cachemem.h"
#include "proj1/diskimg.h"


int cacheMemSizeInKB = 0;
int sectorsAvail = 0;
void *cacheMemPtr;
_LHASH *cachehash;

typedef struct CacheHashEntry {
  unsigned long sectorNum;    // The sector number
  void *sector;   // Where it is located in cache memory
} CacheHashEntry;


/*
 * Hash table callbacks
 */
static unsigned long
HashCallback(const void *arg)
{
  CacheHashEntry *hash_entry = (CacheHashEntry *) arg;
  return hash_entry->sectorNum;
}

static int
CompareCallback(const void *arg1, const void *arg2)
{
 	CacheHashEntry *e1 = (CacheHashEntry *) arg1;
	CacheHashEntry *e2 = (CacheHashEntry *) arg2;
	if(e1->sectorNum == e2->sectorNum) { return 0; }
	return 1;
}

/*
 * Allocate memory of the specified size for the data cache optimizations
 * Return -1 on error, 0 on success. 
 */
int
CacheMem_Init(int sizeInKB)
{
	/* Size needs to be not negative or too big and 
	 * multiple of the 4KB page size 
	 */
	if ((sizeInKB < 0) || (sizeInKB > (CACHEMEM_MAX_SIZE/1024))
	    || (sizeInKB % 4)) {
	  fprintf(stderr, "Bad cache size %d\n", sizeInKB);
	  return -1;
	}
	void *memPtr = mmap(NULL, sizeInKB*1024, PROT_READ|PROT_WRITE, 
	      MAP_PRIVATE|MAP_ANON, -1, 0);
	if (memPtr == MAP_FAILED) {
	  perror("mmap");
	  return -1;
	}
	sectorsAvail = (sizeInKB * 1024) / DISKIMG_SECTOR_SIZE; //ok b/c always a multiple of 512
	cacheMemSizeInKB = sizeInKB;
	cacheMemPtr = memPtr;
	cachehash = lh_new(HashCallback, CompareCallback);
	return 0;
}

/*
 * Returns the total cache size in kilobytes
 */
int 
totalCacheSize(){
	return cacheMemSizeInKB;
}

/*
 * Places a disk sector in the cache
 * Returns 1 if sucessful and 0 if not.
 */
int 
putSectorInCache(struct unixfilesystem *fs, int sectorNum){
	//if space
	if(sectorsAvail > 0){
		//add sector to hash table
		CacheHashEntry *entry = malloc(sizeof(CacheHashEntry));
	    if (entry == NULL) {
			printf("malloc problem\n");
	      	return 0;
	    }
		entry->sectorNum = (unsigned long) sectorNum;
		//read sector data into next cacheMemPtr slot
		if (diskimg_readsector(fs->dfd, sectorNum, cacheMemPtr) != DISKIMG_SECTOR_SIZE) {
		    fprintf(stderr, "Error reading block\n");
		    return -1;
		}
		entry->sector = cacheMemPtr;
		if (entry->sector == NULL) {
		  	free(entry);
			printf("cacheMemPtr problem \n");
		  	return 0;
		}

	    lh_insert(cachehash,(char *) entry);

	    if (lh_error(cachehash)) {
	    	free(entry);
			printf("hash problem\n");
	    	return 0;
	    }
		
		//increment cacheMemPtr to next block of open memory
		cacheMemPtr += DISKIMG_SECTOR_SIZE;	    
		//decrement sectors available
		sectorsAvail--;
		return 1;
	}
	//else
	//replace some sector
	return 0;
}

/*
 * Gets a sector from the cache. 
 * Reads the sector into a large enough buffer of DISKIMG_SECTOR_SIZE so
 * that it's behavior is analogous to diskimg_readsector.
 * Returns 1 if sector was in cache and 0 if not.
 */
int 
getSectorFromCache(int sectorNum, void *buf){
	//hash table lookup
	CacheHashEntry key;
	key.sectorNum = sectorNum;
	CacheHashEntry *entry = lh_retrieve(cachehash, (char *) &key);
	if(entry != NULL){
		//read into buffer if exists
		memcpy(buf, entry->sector, DISKIMG_SECTOR_SIZE);
		return 1;
	}
	//else
	//return 0
	return 0;
}

//quick lookup of sector in cache memory
//quick ability to choose a sector to delete based on meta data
//secondary data structure with sorted list of hash table entries?




