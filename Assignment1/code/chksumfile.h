#ifndef _CHKSUMFILE_H
#define _CHKSUMFILE_H


#include "unixfilesystem.h"

#define CHKSUMFILE_SIZE 20   
#define CHKSUMFILE_STRINGSIZE ((2*CHKSUMFILE_SIZE)+1)


int chksumfile_byinumber(struct unixfilesystem *fs, int inumber, void *chksum);
int chksumfile_bypathname(struct unixfilesystem *fs, const char *pathname, void *chksum);
void chksumfile_cvt2string(void *chksum, char *outstring);
int chksumfile_compare(void *chksum1, void *chksum2);


#endif /* chksumfile.h */
