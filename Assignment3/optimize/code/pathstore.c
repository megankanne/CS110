/*
 * pathstore.c  - Store pathnames for indexing
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <inttypes.h>
#include <openssl/lhash.h>

#if defined(__APPLE__)
  #define _LHASH LHASH
#endif

#include "index.h"
#include "fileops.h"
#include "pathstore.h"
#include "proj1/chksumfile.h"

typedef struct PathstoreElement {
  char chksum[CHKSUMFILE_SIZE];
  char *pathname;
} PathstoreElement;

static uint64_t numdifferentfiles = 0;
static uint64_t numsamefiles = 0;
static uint64_t numdiffchecksum = 0;
static uint64_t numdups = 0;
static uint64_t numcompares = 0;
static uint64_t numstores = 0;
static int numfilesseen = 0;

//static int SameFileIsInStore(Pathstore *store, char *pathname);
//static int IsSameFile(Pathstore *store, char *pathname1, char *pathname2);

// static _LHASH *hashTable = NULL;

static unsigned long
HashCallback(const void *arg)
{
  PathstoreElement *hash_entry = (PathstoreElement *) arg;
  return lh_strhash(hash_entry->chksum);
}

static int
CompareCallback(const void *arg1, const void *arg2)
{
  PathstoreElement *e1 = (PathstoreElement *) arg1;
  PathstoreElement *e2 = (PathstoreElement *) arg2;
  return !(chksumfile_compare(e1->chksum, e2->chksum));
}

static void 
StoreCleanup(void *arg1)
{
	PathstoreElement *e1 = (PathstoreElement *) arg1;
	free(e1->pathname);
}

Pathstore*
Pathstore_create(void *fshandle)
{
	Pathstore *store = malloc(sizeof(Pathstore));
	if (store == NULL)
	  return NULL;

	store->fshandle = fshandle;

	return store;
}

/*
 * Free up all the sources allocated for a pathstore.
 */
void
Pathstore_destory(Pathstore *store)
{
	_LHASH *hashtable = (_LHASH*) (store->elementList);
	
	lh_doall(hashtable, StoreCleanup);
	
	lh_free(hashtable);
}


/*
 * Store a pathname in the pathname store.
 */
char*
Pathstore_path(Pathstore *store, char *pathname, int discardDuplicateFiles)
{
	char chksum1[CHKSUMFILE_SIZE];
	struct unixfilesystem *fs = (struct unixfilesystem *) (store->fshandle);

	numstores++;
	
	PathstoreElement *entry;
	
	/* For 1 file case
	 * No hash table or checksum 
	 */
	if(numfilesseen == 0){
		numfilesseen++;
		entry = malloc(sizeof(PathstoreElement));;
	    entry->pathname = strdup(pathname);
		if (entry->pathname == NULL) {
		  	free(entry);
			printf("memory problem 2\n");
		  	return NULL;
		}
		store->elementList = entry;
		return entry->pathname;
	}
	
	/* For >1 file
	 * Use hash table and checksums
	 */
	_LHASH *hashtable;
	
	// if we are going from 1 file case to 2 files
	if(numfilesseen == 1){
		numfilesseen++;
		//store first entry somewhere
		PathstoreElement *temp = store->elementList;
		//calc checksum for file 1 path
		int err = chksumfile_bypathname(fs, temp->pathname, chksum1);
		if (err < 0) {
	    	fprintf(stderr,"Can't checksum path %s\n", pathname);
		    return 0;
	 	}
		memcpy(temp->chksum, chksum1, CHKSUMFILE_SIZE);
		//initialize hash table
		store->elementList = lh_new(HashCallback, CompareCallback);
		hashtable = (_LHASH*) (store->elementList);
		//seed hash table with first entry
		lh_insert(hashtable,(char *) temp);
	    if (lh_error(hashtable)) {
	    	free(temp);
			printf("hash problem\n");
	    	return NULL;
	    }
	}else{
		hashtable = (_LHASH*) (store->elementList);
	}
	// calc checksum of pathname
	int err = chksumfile_bypathname(fs, pathname, chksum1);
	if (err < 0) {
    	fprintf(stderr,"Can't checksum path %s\n", pathname);
	    return 0;
 	}	

	PathstoreElement key;
	memcpy(key.chksum, chksum1, CHKSUMFILE_SIZE);	
	
	// if discardDups, see if its in table, if it is, return
	if (discardDuplicateFiles) {
		entry = lh_retrieve(hashtable, (char *) &key);
		if(entry != NULL){
			numdups++;
			return NULL;
		}
	}
	
	// otherwise add
	entry = malloc(sizeof(PathstoreElement));
    if (entry == NULL) {
		printf("memory problem\n");
      	return NULL;
    }
	memcpy(entry->chksum, chksum1, CHKSUMFILE_SIZE);
    entry->pathname = strdup(pathname);
	if (entry->pathname == NULL) {
	  	free(entry);
		printf("memory problem 2\n");
	  	return NULL;
	}

    lh_insert(hashtable,(char *) entry);

    if (lh_error(hashtable)) {
    	free(entry);
		printf("hash problem\n");
    	return NULL;
    }
  
	return entry->pathname;
}

void
Pathstore_dumpstats(FILE *file)
{
  fprintf(file,
          "Pathstore:  %"PRIu64" stores, %"PRIu64" duplicates\n"
          "Pathstore2: %"PRIu64" compares, %"PRIu64" checksumdiff, "
          "%"PRIu64" comparesuccess, %"PRIu64" comparefail\n",
          numstores, numdups, numcompares, numdiffchecksum,
          numsamefiles, numdifferentfiles);
}
