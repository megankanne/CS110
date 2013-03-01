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

//#if defined(__APPLE__)
  #define _LHASH LHASH
//#endif

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
//static uint64_t numbychar = 0;

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
StoreCleanup(const void *arg1)
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

	//initialize hash table
	store->elementList = lh_new(HashCallback, CompareCallback);;
	store->fshandle = fshandle;
	//hashTable = (_LHASH*) (store->elementList);

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
	
	//do I need to free the entry as well? how?

	lh_free(hashtable);
}

/*
 * Store a pathname in the pathname store.
 */
char*
Pathstore_path(Pathstore *store, char *pathname, int discardDuplicateFiles)
{
	_LHASH *hashtable = (_LHASH*) (store->elementList);
	char chksum1[CHKSUMFILE_SIZE];
	
	struct unixfilesystem *fs = (struct unixfilesystem *) (store->fshandle);

	numstores++;
	
	// calc checksum of pathname
	int err = chksumfile_bypathname(fs, pathname, chksum1);
	if (err < 0) {
    	fprintf(stderr,"Can't checksum path %s\n", pathname);
	    return 0;
 	}
	//printf("chksum: %s\n", chksum1);
	//printf("Pathstore: %s\n", pathname);
	
	// see if that checksum is already in the data structure?
	PathstoreElement key;
	memcpy(key.chksum, chksum1, CHKSUMFILE_SIZE);
	
	PathstoreElement *entry;
	
	// if discardDups, see if its in table, if it is, return
	if (discardDuplicateFiles) {
		entry = lh_retrieve(hashtable, (char *) &key);
		if(entry != NULL){
			numdups++;
			return NULL;
		}
	}
	//printf("adding %s\n", pathname);
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

	// if (discardDuplicateFiles) {
	// 	  if (SameFileIsInStore(store,pathname)) {
	// 	    numdups++;
	// 	    return NULL;
	// 	  }
	// 	}
	// 
	// 	e = malloc(sizeof(PathstoreElement));
	// 	if (e == NULL) {
	// 	  return NULL;
	// 	}
	// 
	// 	e->pathname = strdup(pathname);
	// 	if (e->pathname == NULL) {
	// 	  free(e);
	// 	  return NULL;
	// 	}
	// 	e->nextElement = store->elementList;
	// 	store->elementList = e;
	// 
	// 	return e->pathname;

}

// /*
//  * Is this file the same as any other one in the store
//  */
// static int
// SameFileIsInStore(Pathstore *store, char *pathname)
// {
//   PathstoreElement *e = store->elementList;
// 
//   while (e) {
//     if (IsSameFile(store, pathname, e->pathname)) {
//       return 1;  // In store already
//     }
//     e = e->nextElement;
//   }
//   return 0; // Not found in store
// }
// 
// /*
//  * Do the two pathnames refer to a file with the same contents.
//  */
// static int
// IsSameFile(Pathstore *store, char *pathname1, char *pathname2)
// {
// 
//   char chksum1[CHKSUMFILE_SIZE],
//        chksum2[CHKSUMFILE_SIZE];
// 
//   struct unixfilesystem *fs = (struct unixfilesystem *) (store->fshandle);
// 
//   numcompares++;
//   if (strcmp(pathname1, pathname2) == 0) {
//     return 1; // Same pathname must be same file.
//   }
// 
//   /* Compute the chksumfile of each file to see if they are the same */
// 
//   int err = chksumfile_bypathname(fs, pathname1, chksum1);
//   if (err < 0) {
//     fprintf(stderr,"Can't checksum path %s\n", pathname1);
//     return 0;
//   }
//   err = chksumfile_bypathname(fs, pathname2, chksum2);
//   if (err < 0) {
//     fprintf(stderr,"Can't checksum path %s\n", pathname2);
//     return 0;
//   }
// 
//   if (chksumfile_compare(chksum1, chksum2) == 0) {
//     numdiffchecksum++;
//     return 0;  // Checksum mismatch, not the same file
//   }
// 
//   /* Checksums match, do a content comparison */
//   int fd1 = Fileops_open(pathname1);
//   if (fd1 < 0) {
//     fprintf(stderr, "Can't open path %s\n", pathname1);
//     return 0;
//   }
// 
//   numsamefiles++;
// }

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
