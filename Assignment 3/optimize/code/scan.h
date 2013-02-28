#ifndef _SCAN_H
#define _SCAN_H

#include "index.h"
#include "pathstore.h"
#include <stdio.h>

int Scan_TreeAndIndex(char *pathname, Index *ind, Pathstore *store,
		      int discardDups);

void Scan_dumpstats(FILE *file);

#endif /* _SCAN_H */
