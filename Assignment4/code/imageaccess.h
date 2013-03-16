#ifndef _IMAGEACCESS_H
#define _IMAGEACCESS_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_IMAGESERVERS 32

int ImageAccess_Init(char *diskSearchLoc, char *imageDirLoc);
int ImageAccess_Start(char *imageFile);
int ImageAccess_Lookup(char *imageFile, struct sockaddr_in *inaddrPtr);


#endif /* _IMAGEACCESS_H */
