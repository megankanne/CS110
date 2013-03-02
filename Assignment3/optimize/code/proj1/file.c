#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "file.h"
#include "inode.h"
#include "diskimg.h"


/*
 * Return the valid number of bytes in the block. If it's the last block,
 * if the filesize isn't a multiple of DISKIMG_SECTOR_SIZE, return the remainder
 * otherwise return the DISKIMG_SECTOR_SIZE. 
 */
int validBytes(struct inode *inp, int blockNum){
	int size = inode_getsize(inp);
	int partial = (size % DISKIMG_SECTOR_SIZE == 0) ? 0 : 1;
	int blocks = (size / DISKIMG_SECTOR_SIZE) + partial;
	if(blockNum == blocks - 1){
		if(partial) { return (size % DISKIMG_SECTOR_SIZE);}
		return DISKIMG_SECTOR_SIZE;
	}else{
		return DISKIMG_SECTOR_SIZE;
	}
}

/*
 * Fetch the specified file block from the specified inode.
 * Return the number of valid bytes in the block, -1 on error.
 */
int
file_getblock(struct unixfilesystem *fs, int inumber, int blockNum, void *buf)
{
	//Find the inode
	//printf("fs->dfd %i init\n", fs->dfd);
	struct inode inp;
	if(inode_iget(fs, inumber, &inp) < 0){
		printf("Error getting inode %i\n", inumber);
		return -1;
	}
	//printf("fs->dfd %i after iget\n", fs->dfd);
	//Find the sector number for the blockNum
	int sectorNum = inode_indexlookup(fs, &inp, blockNum);
	if(sectorNum == -1){
		printf("Error getting data sector %i\n", blockNum);
		return -1;
	}
	//printf("fs->dfd %i after lookup\n", fs->dfd);
	//printf("getblock inumber %i blocknum %i sector %i\n", inumber, blockNum, sectorNum);
	//if(blockNum > 5) assert(0);
	//Read the data in the sector into the buffer
	if (diskimg_readsector(fs->dfd, sectorNum, buf) != DISKIMG_SECTOR_SIZE) {
	    fprintf(stderr, "Error reading sector %i inumber %i block %i\n", sectorNum, inumber, blockNum);
	    return -1;
	}
	//Return number of valid bytes in the block
	return validBytes(&inp, blockNum);
}
