
#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include "file.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>


/*
 * Lookup the specified name (name) in the directory (dirinumber). If found, return the 
 * directory entry in dirEnt. Return 0 on success and something negative on failure. 
 */
int
directory_findname(struct unixfilesystem *fs, const char *name,
                   int dirinumber, struct direntv6 *dirEnt)
{
	// Consider a directory with inumber = 26, filename = “paper”:
	// 	– Find the inode of the directory
	
	//block instance b
	//inode instance i <- INODE_NUMBER_TO_INODE (dir)
	int dirs_per_sector = DISKIMG_SECTOR_SIZE/sizeof(struct direntv6);
	
	struct inode dir;
	struct direntv6 buffer[dirs_per_sector];
	int sentinel = 0;
	
	//Test that directory number given is a directory and allocated
	if(inode_iget(fs, dirinumber, &dir) < 0){
		printf("Error getting inode %i\n", dirinumber);
		return -1;
	}
	if((dir.i_mode & IFMT) != IFDIR || !(dir.i_mode & IALLOC)) { 
		printf("inode %i not a directory or is not allocated\n", dirinumber);
		return -1; 
	}
	
	int filesize = inode_getsize(&dir);
	int partial = (filesize % DISKIMG_SECTOR_SIZE == 0) ? 0 : 1;
	int blocks = (filesize / DISKIMG_SECTOR_SIZE) + partial;
	
	//for each block in the directory
	for(int i = 0; i < blocks; i++){ //range?
		if(sentinel){ break; }
		//place block in buffer
		if(file_getblock(fs, dirinumber, i, &buffer) < 0){
			printf("Problem getting directory %i block %i\n", dirinumber, i);
		}
		//for each direntv6 struct in block
		for(int j = 0; j < dirs_per_sector; j++){
			//compare stuct.name to name
			printf("Looking: %s Found: %s\n", name, buffer[j].d_name);
			if(strncmp(name, buffer[j].d_name, 14) == 0){
				//place content in dirEnt
				*dirEnt = buffer[j];
				printf("equal\n");
				sentinel = 1;
				break;
			}
		}
	}
	if(sentinel){ return 0; }
	
	//fprintf(stderr, "directory_lookupname(name=%s dirinumber=%d)\n", name, dirinumber); 
	return -1;

	// 	– Traverse its data blocks
	// 	– Compare the given filename with directory entries’
	// 	– Get the matched entry

}
