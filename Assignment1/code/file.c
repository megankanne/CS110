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
	int partial = (inode_getsize(inp) % DISKIMG_SECTOR_SIZE == 0) ? 0 : 1;
	int blocks = (inode_getsize(inp) / DISKIMG_SECTOR_SIZE) + partial;
	if(blockNum == blocks - 1){
		if(partial) { return (inode_getsize(inp) % DISKIMG_SECTOR_SIZE);}
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
	//Read the data in the sector into the buffer
	if (diskimg_readsector(fs->dfd, sectorNum, buf) != DISKIMG_SECTOR_SIZE) {
	    fprintf(stderr, "Error reading sector %i for block %i\n", sectorNum, blockNum);
	    return -1;
	}
	//Return number of valid bytes in the block
	return validBytes(&inp, blockNum);
}
