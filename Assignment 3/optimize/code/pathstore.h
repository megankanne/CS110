#ifndef _PATHSTORE_H
#define _PATHSTORE_H

typedef struct Pathstore {
  struct PathstoreElement *elementList;
  void                    *fshandle;
} Pathstore;

Pathstore* Pathstore_create(void *fshandle);
void       Pathstore_destory(Pathstore *store);
char*      Pathstore_path(Pathstore *store, char *pathname,
                          int discardDuplicateFiles);

void Pathstore_dumpstats(FILE *file);

#endif /* _PATHSTORE_H */
