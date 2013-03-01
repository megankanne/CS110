#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "inode.h"
#include "diskimg.h"
#include "../cachemem.h"


#define INODES_PER_SECTOR (DISKIMG_SECTOR_SIZE / sizeof(struct inode))
#define NUM_PTRS_PER_SECTOR (DISKIMG_SECTOR_SIZE/sizeof(uint16_t))

int cachehits = 0;


/*
 * Return the sector containing the inode 
 */
int FindSector(int inumber){
	int sector = inumber / INODES_PER_SECTOR;
	//multiples of 16 are clamped down
	if(inumber % INODES_PER_SECTOR == 0) { sector--; }	
	return INODE_START_SECTOR + sector;	
}


/*
 * Fetch the specified inode from the filesystem. 
 * Return 0 on success, -1 on error.  
 */
int
inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp)
{
	//printf("iget %i inumber\n", inumber);
	int sector = FindSector(inumber);
	//Check sector is an inode sector
	if(sector > fs->superblock.s_isize + INODE_START_SECTOR){
		printf("Sector %i of inodes doesn't exist. Non-existant inode?\n", sector);
		return -1;
	}
	
	struct inode buffer[INODES_PER_SECTOR];
	//check to see if sector is cached, if not cache it and...
	if(!getSectorFromCache(sector, buffer)){
		putSectorInCache(fs, sector);
		//...load that sector into memory
		if (diskimg_readsector(fs->dfd, sector, buffer) != DISKIMG_SECTOR_SIZE) {
		    fprintf(stderr, "Error reading block\n");
		    return -1;
		}
	}else{
		//printf("sector %i found in cache!\n", sector);
		cachehits++;
	}
	int offset = (inumber % INODES_PER_SECTOR) - 1; //-1 b/c offset starts at 0
	if(inumber % INODES_PER_SECTOR == 0) { offset = INODES_PER_SECTOR - 1; } //b/c multiples of 16 are at end
	*inp = buffer[offset];
	
	//if(cachehits % 10 == 0) { printf("cachehits:%i", cachehits); }
	
	return 0;
}

/*
 * Get the sector for the blockNum from the indirect_block  
 */
int GetBlockFromList(int blockNum, uint16_t *indirect_block){
	int offset = (blockNum % NUM_PTRS_PER_SECTOR);
	return *(indirect_block + offset);
}


/*
 * Get the location of the specified file block of the specified inode.
 * Return the disk block number on success, -1 on error.  
 *
 * If small file, returns block inumber directly. If large file, loads the
 * singly (and doubly if necessary) indirect blocks into memory and grabs
 * the appropriate inumber from the pointer.
 */
int
inode_indexlookup(struct unixfilesystem *fs, struct inode *inp, int blockNum)
{
	//printf("index loopkup %i block number\n", blockNum);
	//Check small or large
	if((inp->i_mode & ILARG) == 0){
		return inp->i_addr[blockNum];	
	}else{
		int indirect_block_ptr = blockNum / NUM_PTRS_PER_SECTOR;
		//clamp doubly indirect blocks
		if(indirect_block_ptr > 7){ indirect_block_ptr = 7; }
		int indirect_block_sector = inp->i_addr[indirect_block_ptr];
		uint16_t indirect_block[NUM_PTRS_PER_SECTOR];
		//check to see if sector is cached, if not cache it and...
		if(!getSectorFromCache(indirect_block_sector, indirect_block)){
			putSectorInCache(fs, indirect_block_sector);
			//...load that sector into memory
			if (diskimg_readsector(fs->dfd, indirect_block_sector, indirect_block) != DISKIMG_SECTOR_SIZE) {
			    fprintf(stderr, "Error reading indirect block\n");
			    return -1;
			}
		}else{
			//printf("sector %i found in cache!\n", sector);
			cachehits++;
		}
		//Check singly or doubly indirect
		if(indirect_block_ptr < 7){
			return GetBlockFromList(blockNum, indirect_block);
		}else{
			//printf("doubly indirect!\n");
			indirect_block_ptr = (blockNum-(NUM_PTRS_PER_SECTOR * 7))/NUM_PTRS_PER_SECTOR;
			indirect_block_sector = GetBlockFromList(indirect_block_ptr, indirect_block);
			uint16_t sec_indirect_block[NUM_PTRS_PER_SECTOR];
			if(!getSectorFromCache(indirect_block_sector, sec_indirect_block)){
				putSectorInCache(fs, indirect_block_sector);
				//...load that sector into memory
				if (diskimg_readsector(fs->dfd, indirect_block_sector, sec_indirect_block) != DISKIMG_SECTOR_SIZE) {
				    fprintf(stderr, "Error reading indirect block\n");
				    return -1;
				}
			}else{
				//printf("sector %i found in cache!\n", sector);
				cachehits++;
			}
			return GetBlockFromList(blockNum, sec_indirect_block);
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
