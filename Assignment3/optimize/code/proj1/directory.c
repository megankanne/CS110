
#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include "file.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../cachemem.h"


#define DIRS_PER_SECTOR (int)(DISKIMG_SECTOR_SIZE/sizeof(struct direntv6))

/* 
 * Returns the number of blocks an inode uses
 */
int getBlocks(struct inode *dir){
	int filesize = inode_getsize(dir);
	int partial = (filesize % DISKIMG_SECTOR_SIZE == 0) ? 0 : 1;
	return (filesize / DISKIMG_SECTOR_SIZE) + partial;
}

/*
 * Lookup the specified name (name) in the directory (dirinumber). If found, return the 
 * directory entry in dirEnt. Return 0 on success and something negative on failure.
 *
 * Tests that directory given is a valid inode, is allocated, and is a directory.
 * Reads blocks pointed to by directory into a buffer then checks each struct 
 * direntv6 in the block for a directory named 'name'. If found, returns directory in
 * dirEnt. 
 */
int
directory_findname(struct unixfilesystem *fs, const char *name,
                   int dirinumber, struct direntv6 *dirEnt)
{	
	struct inode dir;
	struct direntv6 buffer[DIRS_PER_SECTOR];
	//Test that directory number given is a directory and is allocated
	if(inode_iget(fs, dirinumber, &dir) < 0){
		printf("Error getting inode %i\n", dirinumber);
		return -1;
	}
	if((dir.i_mode & IFMT) != IFDIR || !(dir.i_mode & IALLOC)) { 
		printf("inode %i not a directory or is not allocated\n", dirinumber);
		return -1; 
	}
	int blocks = getBlocks(&dir);
	//for each block in the directory, place block in buffer
	for(int i = 0; i < blocks; i++){
		//printf("getblock %i sector %i inumber %i blockNum\n", sectorNum, dirinumber, i);
			if(!getSectorFromCache(0, &buffer, dirinumber, i, 0, fs)){
				putSectorInCache(fs, 0, dirinumber, i, 0);
				//...load that sector into memory
				if(file_getblock(fs, dirinumber, i, &buffer) < 0){
					printf("Problem getting directory %i block %i\n", dirinumber, i);
					return -1;
				}
			}
	
		//for each direntv6 struct in block, compare struct.name to name, place content in dirEnt
		for(int j = 0; j < DIRS_PER_SECTOR; j++){
			if(strncmp(name, buffer[j].d_name, 14) == 0){
				*dirEnt = buffer[j];
				return 0;
			}
		}
	}
	return -1;
}
