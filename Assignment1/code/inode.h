#ifndef _INODE_H
#define _INODE_H

#include "unixfilesystem.h"

int inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp); 
int inode_indexlookup(struct unixfilesystem *fs, struct inode *inp, int blockNum);
int inode_getsize(struct inode *inp);



#endif /* _INODE_ */
