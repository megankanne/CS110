
#include "pathname.h"
#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>


/*
 * Return 1 if pathcpy is a path otherwise 0. Also splits the string
 * at the / so that anything before the / is in pathcpy and anything
 * after is in rest.
 */
int isPath(char **pathcpy, char *rest){
	char *result = strsep(pathcpy, "/");
	printf("result: %s pathcpy: %s\n", result, *pathcpy);
	strcpy(rest, result);
	return *pathcpy == NULL ? 0 : 1;
	
}

/*
 * Recursive function to determine the inumber of a given path. Creates two
 * buffers. pathcpy is a copy of the path and buf will hold the rest after
 * tokenization. 
 */
int path_to_inode(struct unixfilesystem *fs, const char *pathname, int dir){
	struct direntv6 dirEnt;
	char pathcpy[strlen(pathname) + 1];
	char buf[strlen(pathname) + 1];
	strncpy(pathcpy, pathname, strlen(pathname) + 1);
	char *rest = pathcpy;
	char *first = buf;
	
	if(isPath(&rest, first) == 0){
		printf("Rest ~isPath: %s\n", rest);
		printf("First ~isPath: %s\n", first);
		
		
		if(first[0] == '\0'){ return dir; }

		//find directory entry		
		if(directory_findname(fs, first, dir, &dirEnt) == -1){
			printf("Failed to find %s in directory %i\n", pathname, dir);
			return -1;
		}
		//return the inode
		return dirEnt.d_inumber;
	}else{
		printf("Rest isPath: %s\n", rest);
		printf("First isPath: %s\n", first);
		
		if(directory_findname(fs, first, dir, &dirEnt) == -1){
			printf("Failed to find %s in directory %i\n", pathname, dir);
			return -1;
		}
		return path_to_inode(fs, rest, dirEnt.d_inumber);
	}	
}


/*
 * Return the inumber associated with the specified pathname. This need only
 * handle absolute paths. Return a negative number if an error is encountered.
 */
int
pathname_lookup(struct unixfilesystem *fs, const char *pathname)
{
	printf("Checking %s\n", pathname);
	if(pathname[0] == '/') { 
		pathname++;
		return path_to_inode(fs, pathname, ROOT_INUMBER); 
	}else{
		printf("Error: Not absolute path: %s\n", pathname);
		return -1;
	}
}
