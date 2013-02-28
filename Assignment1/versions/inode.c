#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "inode.h"
#include "diskimg.h"

#define INODES_PER_SECTOR (DISKIMG_SECTOR_SIZE / sizeof(struct inode))


int FindSector(int inumber){
	//What is the sector offset where the inode resides?
	int sector = inumber / INODES_PER_SECTOR;
	//multiples of 16 are clamped down
	if(inumber % INODES_PER_SECTOR == 0) { sector--; }	
	//What is the absolute sector? 
	return INODE_START_SECTOR + sector;
	
}


/*
 * Fetch the specified inode from the filesystem. 
 * Return 0 on success, -1 on error.  
 */
int
inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp)
{
	//printf("\ninumber: %i\n", inumber);
	//find sector that needs to be loaded into memory
	int sector = FindSector(inumber);
	//check sector is an inode sector
	if(sector > fs->superblock.s_isize + INODE_START_SECTOR){
		printf("Sector %i of inodes doesn't exist. Non-existant inode?\n", sector);
		return -1;
	}
	//load that sector into memory
	struct inode *ilist = (struct inode*) malloc(DISKIMG_SECTOR_SIZE);
	//printf("sector: %i\n", sector);
	if (diskimg_readsector(fs->dfd, sector, ilist) != DISKIMG_SECTOR_SIZE) {
	    fprintf(stderr, "Error reading block\n");
	    return -1;
	}
	//point *inp to the correct inode in the allocated sector
	int offset = (inumber % INODES_PER_SECTOR) - 1; //-1 b/c offset starts at 0
	if(inumber % INODES_PER_SECTOR == 0) { offset = INODES_PER_SECTOR - 1; } //b/c multiples of 16 are at end
	//printf("offset: %i\n", offset);
	*inp = *(ilist + offset);
	return 0;
}

/*
 * Get the sector for the blockNum from the indirect_block  
 */
int GetBlockFromList(int blockNum, int num_ptrs_per_sector, uint16_t *indirect_block){
	//get offset in sector
	uint16_t *temp;
	int offset = (blockNum % num_ptrs_per_sector); 
	//printf("offset: %i\n", offset);
	//increment temp by offset
	temp = (indirect_block + offset);
	//printf("Block %i at sector %i on disk\n", blockNum, *temp);
	//return the block listed at the offset
	return *temp;
}


/*
 * Get the location of the specified file block of the specified inode.
 * Return the disk block number on success, -1 on error.  
 */
int
inode_indexlookup(struct unixfilesystem *fs, struct inode *inp, int blockNum)
{
	//printf("blockNum: %i\n", blockNum);
	//  – Is the inode a small or large file?
	if(inode_getsize(inp) < (DISKIMG_SECTOR_SIZE * 8)){
		// 	– Small files use 8 direct data block pointers
		//printf("Small\n");
		//printf("Block %i at sector %i on disk\n", blockNum, inp->i_addr[blockNum]);
		return inp->i_addr[blockNum];
		
	}else{
		// 	– Large files use 7 indirect block pointers and one doubly indirect block pointer
		//printf("Large\n");
		int num_ptrs_per_sector = DISKIMG_SECTOR_SIZE/sizeof(uint16_t);
		//get correct indirect block index for blockNum
		int indirect_block_ptr = blockNum / num_ptrs_per_sector;
		//printf("raw indirect block ptr: %i\n", indirect_block_ptr);
		if(indirect_block_ptr > 7){ indirect_block_ptr = 7; }
		//printf("capped indirect block ptr: %i\n", indirect_block_ptr);
		//get indirect block sector value from array
		int indirect_block_sector = inp->i_addr[indirect_block_ptr];
		//load into memory the indirect block sector specified
		uint16_t *indirect_block = (uint16_t *) malloc(DISKIMG_SECTOR_SIZE);
		if (diskimg_readsector(fs->dfd, indirect_block_sector, indirect_block) != DISKIMG_SECTOR_SIZE) {
		    fprintf(stderr, "Error reading indirect block\n");
		    return -1;
		}
		//if singly indirect pointer
		if(indirect_block_ptr < 7){
			return GetBlockFromList(blockNum, num_ptrs_per_sector, indirect_block);
		}else{
			//find the second indirect ptr
			indirect_block_ptr = (blockNum-(num_ptrs_per_sector * 7))/num_ptrs_per_sector;
			//printf("blockNum: %i\n", blockNum);
			//printf("secondary indirect block ptr: %i\n", indirect_block_ptr);
			//get secondry indirect block sector value from first indirect block
			indirect_block_sector = GetBlockFromList(indirect_block_ptr, num_ptrs_per_sector, indirect_block);
			//printf("secondary indirect block sector: %i\n", indirect_block_sector);
			//load into memory the doubly indirect blocks specified
			uint16_t *sec_indirect_block = (uint16_t *) malloc(DISKIMG_SECTOR_SIZE);
			if (diskimg_readsector(fs->dfd, indirect_block_sector, sec_indirect_block) != DISKIMG_SECTOR_SIZE) {
			    fprintf(stderr, "Error reading indirect block\n");
			    return -1;
			}
			//printf("sector returned: %i\n", GetBlockFromList(blockNum, num_ptrs_per_sector, sec_indirect_block));
			return GetBlockFromList(blockNum, num_ptrs_per_sector, sec_indirect_block);
		}		
	}
	return -1;
}

/* 
 * Compute the size of an inode from its size0 and size1 fields.
 */
int
inode_getsize(struct inode *inp) 
{
  return ((inp->i_size0 << 16) | inp->i_size1); 
}
