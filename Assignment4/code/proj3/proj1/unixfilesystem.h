#ifndef _UNIXFILESYSTEM_H
#define _UNIXFILESYSTEM_H

/* 
 * Include the definitions taken from the Unix sources. 
 */
#include "filsys.h"   /* Superblock definition */
#include "ino.h"      /* Inode definition */  
#include "direntv6.h"   /* Directory entry  */


/*
 * The layout of the Unix disk looked as follows:
 * Sector 0 - The bootblock. The first 16bit word of this sector should be 0407.
 * Sector 1 - The superblock (struct filsys as defined in filsys.h)
 * Sector 2 - The start of the inode area of the disk.  
 *    The field s_isize of superblock tells how many sectors of inodes there are. 
 * Sector 2+s_isize : The rest of the sectors of disk 
 */

#define BOOTBLOCK_SECTOR   0     
#define SUPERBLOCK_SECTOR  1
#define INODE_START_SECTOR 2

#define ROOT_INUMBER       1

#define BOOTBLOCK_MAGIC_NUM 0407


struct unixfilesystem {
  int dfd;   /* Handle from the diskimg module to read/write the diskimg. */
  struct filsys  superblock;   /* The superblock read from the diskimage */
};


struct unixfilesystem *unixfilesystem_init(int fd);


#endif /* _UNIXFILESYSTEM_H */
