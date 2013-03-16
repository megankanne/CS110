/*
 * fileops.c  -  This module provides an Unix like file absraction
 * on the prog1 file system access code
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>

#include "fileops.h"
#include "proj1/pathname.h"
#include "proj1/unixfilesystem.h"
#include "diskimg.h"
#include "proj1/inode.h"
#include "proj1/file.h"
#include "proj1/chksumfile.h"

#define MAX_FILES 64

static uint64_t numopens = 0;
static uint64_t numreads = 0;
static uint64_t numgetchars = 0;
static uint64_t numisfiles = 0;

/*
 * Table of open files.
 */
static struct {
  char *pathname;    // absolute pathname NULL if slot is not used.
  int  cursor;       // Current position in the file
} openFileTable[MAX_FILES];


static struct unixfilesystem *unixfs;

/* Globals that cache the last sector used and its metadata. */
unsigned char buf[DISKIMG_SECTOR_SIZE]; //Buffer for the sector being read.
int lblockNum;
int linum;
int lbytes;
int lfd;
int lsize;

/*
 * Initialize the fileops module for the specified disk.
 */
void *
Fileops_init(char *diskpath)
{
	memset(openFileTable, 0, sizeof(openFileTable));

	int fd = diskimg_open(diskpath, 1);
	if (fd < 0) {
	  fprintf(stderr, "Can't open diskimagePath %s\n", diskpath);
	  return NULL;
	}
	unixfs = unixfilesystem_init(fd);
	if (unixfs == NULL) {
	  diskimg_close(fd);
	  return NULL;
	}
	linum = ROOT_INUMBER - 1;
	lbytes = 0;
	lblockNum = 0;
	lfd = -1;
	lsize = 0;
	return unixfs;
}

/*
 * Open the specified absolute pathname for reading. Returns -1 on error;
 */
int
Fileops_open(char *pathname)
{
  int fd;
  int inumber;

  numopens++;
  inumber = pathname_lookup(unixfs,pathname);
  if (inumber < 0) {
    return -1; // File not found
  }
  for (fd = 0; fd < MAX_FILES; fd++) {
    if (openFileTable[fd].pathname == NULL) break;
  }
  if (fd >= MAX_FILES) {
    return -1;  // No open file slots
  }
  openFileTable[fd].pathname = strdup(pathname); // Save our own copy
  openFileTable[fd].cursor = 0;

  return fd;
}


/*
 * Fetch the next character from the file. Return -1 if at end of file.
 */
int
Fileops_getchar(int fd)
{
	int inumber;
	struct inode in;
	int bytesMoved;
	int err, size;
	int blockNo, blockOffset;

	numgetchars++;

	if (openFileTable[fd].pathname == NULL)
	  return -1;  // fd not opened.

	// If we are reading from the same file 
	if(fd != lfd){
		inumber = pathname_lookup(unixfs, openFileTable[fd].pathname);
		if (inumber < 0) {
		  return inumber; // Can't find file
		}

		err = inode_iget(unixfs, inumber,&in);
		if (err < 0) {
		  return err;
		}
		if (!(in.i_mode & IALLOC)) {
		  return -1;
		}
		size = inode_getsize(&in);
	} else {
		size = lsize;
		inumber = linum;
	}
	
	if (openFileTable[fd].cursor >= size) return -1; // Finished with file

	blockNo = openFileTable[fd].cursor / DISKIMG_SECTOR_SIZE;
	blockOffset =  openFileTable[fd].cursor % DISKIMG_SECTOR_SIZE;

	// If a new inumber and blockNo, read new block into cache
	// else used the previously cached block
	if(inumber != linum || blockNo != lblockNum){
		bytesMoved = file_getblock(unixfs, inumber,blockNo,buf);
	}else{
		bytesMoved = lbytes;
	}
	
	if (bytesMoved < 0) {
	  return -1;
	}
	assert(bytesMoved > blockOffset);

	openFileTable[fd].cursor += 1;
	
	/* Cache the inumber and block number used */
	linum = inumber;
	lblockNum = blockNo;
	lbytes = bytesMoved;
	lfd = fd;
	lsize = size;

	return (int)(buf[blockOffset]);
}

/*
 * Implement the Unix read system call. Number of bytes returned.  Return -1 on
 * err.
 */
int
Fileops_read(int fd, char *buffer, int length)
{
  int i;
  int ch;

  numreads++;

  for (i = 0; i < length; i++) {
    ch = Fileops_getchar(fd);
    if (ch == -1) break;
    buffer[i] = ch;
  }
  return i;
}

/*
 * Return the current position in the file.
 */
int
Fileops_tell(int fd)
{
  if (openFileTable[fd].pathname == NULL)
    return -1;  // fd not opened.

  return openFileTable[fd].cursor;
}


/*
 * Close the files - return the resources
 */

int
Fileops_close(int fd)
{
  if (openFileTable[fd].pathname == NULL)
    return -1;  // fd not opened.

  free(openFileTable[fd].pathname);
  openFileTable[fd].pathname = NULL;
  return 0;
}

/*
 * Return true if specified pathname is a regular file.
 */
int
Fileops_isfile(char *pathname)
{
  numisfiles++;

  int inumber = pathname_lookup(unixfs, pathname);
  if (inumber < 0) {
    return 0;
  }

  struct inode in;
  int err = inode_iget(unixfs, inumber, &in);
  if (err < 0) return 0;

  if (!(in.i_mode & IALLOC) || ((in.i_mode & IFMT) != 0)) {
    /* Not allocated or not a file */
    return 0;
  }
  return 1; /* Must be a file */
}

void
Fileops_dumpstats(FILE *file)
{
  fprintf(file,
          "Fileops: %"PRIu64" opens, %"PRIu64" reads, "
          "%"PRIu64" getchars, %"PRIu64 " isfiles\n",
          numopens, numreads, numgetchars, numisfiles);
}
