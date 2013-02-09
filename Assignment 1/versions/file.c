#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "file.h"
#include "inode.h"
#include "diskimg.h"

/*
 * Fetch the specified file block from the specified inode.
 * Return the number of valid bytes in the block, -1 on error.
 */
int
file_getblock(struct unixfilesystem *fs, int inumber, int blockNum, void *buf)
{
	// Consider inumber = 17, blocknumber = 5: 
	// – Find the inode
	struct inode inp;
	if(inode_iget(fs, inumber, &inp) < 0){
		printf("Error getting inode %i\n", inumber);
		return -1;
	}
	int filesize = inode_getsize(&inp);
	int partial = (filesize % DISKIMG_SECTOR_SIZE == 0) ? 0 : 1;
	int blocks = (filesize / DISKIMG_SECTOR_SIZE) + partial;
	
	// – Find the sector number
	int sectorNum = inode_indexlookup(fs, &inp, blockNum);
	if(sectorNum == -1){
		printf("Error getting data sector %i\n", blockNum);
		return -1;
	}
	if((sectorNum > (int)fs->superblock.s_fsize) || (sectorNum < fs->superblock.s_isize + INODE_START_SECTOR)){
		printf("sectorNum: %i blockNum: %i blocks: %i inode: %i\n", sectorNum, blockNum, blocks, inumber);
		
	}
	// – Read the data in the sector into the buffer
	if (diskimg_readsector(fs->dfd, sectorNum, buf) != DISKIMG_SECTOR_SIZE) {
	    fprintf(stderr, "Error reading sector %i for block %i of %i\n", sectorNum, blockNum, blocks);
	    return -1;
	}
	//Determine value of valid bytes in the block
	if(blockNum == blocks - 1){
		if(partial) {
			return (filesize % DISKIMG_SECTOR_SIZE);
		}else{
			return DISKIMG_SECTOR_SIZE;
		}
	}else{
		return DISKIMG_SECTOR_SIZE;
	}
}
