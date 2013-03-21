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

#if defined(__APPLE__)
  #define _LHASH LHASH
#endif

#include <sys/mman.h>

#include "cachemem.h"
#include "proj1/diskimg.h"
#include "proj1/inode.h"

//Struct for sector metadata
typedef struct EntryMetaData{
	unsigned long sectorNum;
	int added;	//higher indexes are more recent
	int used; //higher indexes are more recent
	int isinode;
} EntryMetaData;

//Struct for sector hashing
typedef struct CacheHashEntry {
  unsigned long sectorNum;    // The sector number
  void *sector;   // Where it is located in cache memory
} CacheHashEntry;

int cacheMemSizeInKB = 0;
int sectorsAvail = 0;
void *cacheMemPtr;
_LHASH *cachehash;
EntryMetaData *metadata;
EntryMetaData *position;
int addedBlks;
int accessedBlks;
int sectorsPossible;
int replaced = 0;
int inumseen = -1;


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
	sectorsPossible = sectorsAvail;
	cacheMemSizeInKB = sizeInKB;
	cacheMemPtr = memPtr;
	cachehash = lh_new(HashCallback, CompareCallback);
	void *memPtr2 = mmap(NULL, sectorsPossible*sizeof(EntryMetaData), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
	if (memPtr2 == MAP_FAILED) {
	  perror("mmap");
	  return -1;
	}
	metadata = memPtr2;
	position = metadata;
	addedBlks = 0;
	accessedBlks = 0;
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
 * Least Recently Used qsort comparison function.
 */
int LRUcomp(const void * a, const void * b){
	EntryMetaData *e1 = (EntryMetaData *) a;
	EntryMetaData *e2 = (EntryMetaData *) b;
	return e1->used - e2->used;
}

/*
 * For the calls from the directory module to get the sector number 
 * because it is not that module can't pass it in. 
 */

int getSectorNum(struct unixfilesystem *fs, int inumber, int blockNum){
	//Find the inode
	struct inode inp;
	if(inode_iget(fs, inumber, &inp) < 0){
		printf("Error getting inode %i\n", inumber);
		return -1;
	}
	//Find the sector number for the blockNum
	int sectorNum = inode_indexlookup(fs, &inp, blockNum);
	if(sectorNum == -1){
		printf("Error getting data sector %i\n", blockNum);
		return -1;
	}
	return sectorNum;
}

/*
 * Places a disk sector in the cache. If we have allocated all of the
 * sectors possible (aka sectorsAvail == 0), then replace some sector.
 * The sector replaced is determined by a quicksort of the sector
 * metadata array such that the LRU is in the front. 
 * Returns 1 if sucessful and 0 if not.
 */
int 
putSectorInCache(struct unixfilesystem *fs, int sectorNum, int inumber, int blockNum,  int inodecall){
	
	if(!inodecall){	sectorNum = getSectorNum(fs, inumber, blockNum); }
	
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
		//add sector metadata to array
		position->sectorNum = (unsigned long) sectorNum;
		position->added = addedBlks;
		position->used = accessedBlks;
		position ++;
	
		addedBlks++;
		accessedBlks++;
		
		cacheMemPtr += DISKIMG_SECTOR_SIZE;	    
		sectorsAvail--;
		return 1;
	}else{
		//replace some sector
		replaced++;
		//sort the metadata array
		qsort(metadata, sectorsPossible, sizeof(EntryMetaData), LRUcomp);
		//grab associated hash table element
		CacheHashEntry key;
		key.sectorNum = metadata->sectorNum;
		CacheHashEntry *entry = lh_retrieve(cachehash, (char *) &key);
		void *sectorptr = entry->sector;
		//remove entry from hash table
		lh_delete(cachehash, entry);
		//create new entry as above except read in the sector to sectorptr
		entry->sectorNum = (unsigned long) sectorNum;
		if (diskimg_readsector(fs->dfd, sectorNum, sectorptr) != DISKIMG_SECTOR_SIZE) {
		    fprintf(stderr, "Error reading block\n");
		    return -1;
		}
		entry->sector = sectorptr;
		if (entry->sector == NULL) {
		  	free(entry);
				printf("sectorptr problem \n");
		  	return 0;
		}
		
		lh_insert(cachehash,(char *) entry);
		
		if (lh_error(cachehash)) {
			    free(entry);
				printf("hash problem\n");
			    return 0;
		}
		//add sector metadata to array
		metadata->sectorNum = (unsigned long) sectorNum;
		metadata->added = addedBlks;
		metadata->used = accessedBlks;
		
		accessedBlks++;
		return 1;
	}
	return 0;
}

/*
 * Gets a sector from the cache. 
 * Reads the sector into a large enough buffer of DISKIMG_SECTOR_SIZE so
 * that it's behavior is analogous to diskimg_readsector.
 * Returns 1 if sector was in cache and 0 if not.
 */
int 
getSectorFromCache(int sectorNum, void *buf, int inumber, int blockNum, int inodecall, struct unixfilesystem *fs){
	if(!inodecall){	sectorNum = getSectorNum(fs, inumber, blockNum); }
	//hash table lookup
	CacheHashEntry key;
	key.sectorNum = sectorNum;
	CacheHashEntry *entry = lh_retrieve(cachehash, (char *) &key);
	if(entry != NULL){
		//read into buffer if exists
		memcpy(buf, entry->sector, DISKIMG_SECTOR_SIZE);
		return 1;
	}
	return 0;
}




