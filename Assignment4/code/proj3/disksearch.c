#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <string.h>

#include <sys/time.h>
#include <sys/resource.h>

#include "index.h"
#include "debug.h"
#include "pathstore.h"
#include "fileops.h"
#include "diskimg.h"
#include "disksim.h"
#include "scan.h"
#include "cachemem.h"


static void PrintUsageAndExit(char *progname);
static void DumpStats(FILE *file);
static void DumpUsageStats(FILE *file);
static int  QueryWord(char *word, Index *ind, FILE *file );

static void BuildDiskIndex(char *diskpath, int discardDups);
static void TestServiceBySingleWord(char *queryWord);
static void TestServiceByFileOfWords(char *queryFile);

/*
 * Gloabl program options
 */
int quietFlag = 0;
int diskLatency = 8000;
int diskBusyWaitEnable = 0;

/*
 * The index store
 */
static Index *diskIndex = NULL;
static Pathstore *store = NULL;

typedef struct packetHeader {
	unsigned int size;
} packetHdr;


#define INFINITE_CACHE_SIZE (64*1024*1024)  /* 64MB is infinite */




/*
 *************************************************************
 * Start of code used in Assignment #4
 *  This code isn't used in assignment #3
 */

int serverAckFd = 0;

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


void ParsePayload(char *payload, char **image, char **word){
	*image = strtok(payload, "?");
	*word = strtok(NULL,"");
}

static void
ProcessQuery(int sock)
{
	// Process request(s) that come in on this socket.

	
	/* Read */
	char buf[512];
	char *loc = buf;
	unsigned int nread = 0;
	//read at least size of packetHdr
	while (nread < sizeof(packetHdr)) {
		int bytes = read(sock, loc, 1);        
        if (bytes < 0) {
            perror("write");
        } 
        nread += bytes;
        loc += bytes;
    }
	//get the packetsize
	packetHdr *header = (packetHdr *)buf;
	unsigned int pktlen = header->size;
	
	char *respbuf = calloc(pktlen, 1);
	if (respbuf == NULL) {
		printf("calloc problem\n");
      	return;
    }
	memcpy(respbuf, buf, nread);
	char *here = respbuf + nread;
	
	unsigned int diff = pktlen-nread;
	//keep reading for packetsize-bytes already read	
	for (unsigned int pos = 0; pos < diff; pos++) {
	    int retval = read(sock, here + pos, 1);
	    if (retval < 0) {
	    	perror("readr");
	    }
		nread += retval;
	}
	char *payload = respbuf;
	payload += sizeof(packetHdr);
	
	//parse packet payload for image and word
	char *image;
	char *word;
	ParsePayload(payload, &image, &word);
	
	
	/* Perform query */
	
	//calloc b/c of fun bug where different calls ask for the same heap chunk after last free
	//which still contains the last response, so strcat contatenates new response onto old :)
	char *response = calloc(128, 1);
	if (response == NULL) {
		printf("calloc problem\n");
      	return;
    } 
	char *data = response + sizeof(packetHdr);
	int allocd = 128; //initial results size
	int rsize = sizeof(packetHdr); //response size
	int qsize = 0;
	
	packetHdr *header2 = (packetHdr *)response;
	
	IndexLocationList *where = Index_RetrieveEntry(diskIndex, word);
	if (where == NULL) {
		qsize = strlen(word) + strlen("Word  not found<br>\n") + 1;
		strcat(data, "Word ");
		strcat(data, word);
		strcat(data, " not found<br>\n");
		rsize += qsize;
	}else{
		while (where) {	
			//deal with converting the offset number to a string
			char offset[16];
			sprintf(offset,"%d", where->item.offset);
			
			qsize = strlen(word) + strlen(where->item.pathname) + strlen(offset) + strlen("Word  @ :<br>\n") + 1;			
			if(rsize + qsize > allocd){
				response = realloc(response, allocd * 2);
				if (response == NULL) {
					printf("calloc problem\n");
			      	return;
			    }
				allocd = allocd * 2;
				data = response + sizeof(packetHdr);
			}
			strncat(data, "Word ", 5);			
			strncat(data, word, strlen(word));			
			strncat(data, " @ ", 3);			
			strncat(data, where->item.pathname, strlen(where->item.pathname)); //overwriting somthing			 
			strncat(data, ":", 1);				
			strncat(data, offset, strlen(offset));			
			strncat(data, "<br>\n", 6);
			//snprintf(response + rsize, qsize, "Word %s @ %s:%d<br>\n", word, where->item.pathname, where->item.offset);
			rsize += qsize;
			where = where->nextLocation;
		}
	}
	header2->size = rsize;
	
		
	/* Write result to socket */
	char *linebuffer;
	linebuffer = response;

	//start sending
    while (rsize > 0) {
        int bytes =  write(sock, linebuffer, rsize);
        if (bytes < 0) {
            perror("write");
        }
        rsize -= bytes;
        linebuffer += bytes;
    }

	// Clean up the socket when done
	close(sock);
	free(response);
}

static void
ServerMode(int serverAckFd)
{
  struct sockaddr_in serv_addr, cli_addr;

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("ERROR opening socket");
    return;
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = 0; // Any port

  int err = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
  if (err < 0)  {
    perror("ERROR on binding");
    close(sockfd);
    return;
  }

  /*
   * Query the socket to see what IP address and port were assign to it.
   */
  socklen_t len = sizeof(serv_addr);
  err = getsockname(sockfd,  (struct sockaddr *) &serv_addr, &len);
  if (err < 0) {
    perror("getsockname");
    close(sockfd);
    return;
  }

  /*
   * Need the socket to listen for incomming connections.
   */
  listen(sockfd,5);

  /*
   * Write the socket address structure down the pipe back
   * to the webserver so it will have this information and
   * also will know we are done indexing and can accept
   * requests.
   */
  err = write(serverAckFd, &serv_addr, sizeof(serv_addr));
  if (err < 0) {
    perror("write");
  }
  close(serverAckFd);  // Don't need this anymore

  /*
   * Go in loop accepting connections and handling them off
   * ProcessQuery to handle.  Note that forking off a child
   * to handle the processing of the query would be a good
   * idea and most likely work. The reason this code doesn't
   * do this is some disksearch implementation use pthreads
   * in a way incompatible with fork.
   */
  while (1) {
    socklen_t clilen = sizeof(cli_addr);
    int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr,  &clilen);
    if (newsockfd < 0)  {
      perror("ERROR on accept");
      close(sockfd);
      return;
    }
    ProcessQuery(newsockfd); // Assume routine closes newsockfd when done
  }
}

/*
 *************************************************************
 * End of code used in Assignment #4
 */


int
main(int argc, char *argv[])
{
  int opt;
  char *queryWord = NULL;
  char *queryFile = NULL;
  int discardDupFiles = 1;
  int cacheSizeInKB = 0;

  while ((opt = getopt(argc, argv, "ql:d:uw:f:bc:s:")) != -1) {
    switch (opt) {
      case 'q':
        quietFlag = 1;
        break;
      case 'l':
        diskLatency = atoi(optarg);
        break;
      case 'c':
        cacheSizeInKB = atoi(optarg);
        break;
      case 'b':
        diskBusyWaitEnable = 1;
        break;
      case 'u':
        discardDupFiles = 0;
        break;
      case 'w':
        queryWord = strdup(optarg);
        break;
      case 'f':
        queryFile = strdup(optarg);
        break;
      case 's':
        serverAckFd = atoi(optarg);
		diskLatency = 0;  /* Default for assignment 3 is zero-latency */
        break;
      case 'd': {
        char *c = optarg;
        while (*c) {
          Debug_SetFlag(*c, 1);
          c++;
        }
        break;
      }
      default:
        PrintUsageAndExit(argv[0]);
    }
  }
  if (optind != argc-1) {
    PrintUsageAndExit(argv[0]);
  }

  /*
   * Allocate Memory for any caching of data. 0 means infinite cache.
   */
  if (cacheSizeInKB == 0) {
    cacheSizeInKB = INFINITE_CACHE_SIZE/1024;  
  }
  int retVal = CacheMem_Init(cacheSizeInKB);
  if (retVal < 0) {
    fprintf(stderr, "Can't allocate memory for cache\n");
    return 1;
  }

  char *diskpath = argv[optind];
  BuildDiskIndex(diskpath, discardDupFiles);

  if (queryWord) {
    TestServiceBySingleWord(queryWord);
  }
  if (queryFile) {
    TestServiceByFileOfWords(queryFile);
  }

  if (serverAckFd) {
    ServerMode(serverAckFd);
  }
  if (!quietFlag) {
    printf("************ Stats ***************\n");
    DumpStats(stdout);
    DumpUsageStats(stdout);
  }

  exit(EXIT_SUCCESS);
  return 0;
}



void
BuildDiskIndex(char *diskpath, int discardDups)
{
  void *fshandle = Fileops_init(diskpath);
  if (fshandle == NULL) {
    fprintf(stderr, "Error initializing  %s\n", diskpath);
    exit(EXIT_FAILURE);
  }

  store = Pathstore_create(fshandle);
  if (store == NULL) {
    fprintf(stderr, "Can't create pathstore\n");
    exit(EXIT_FAILURE);
  }

  diskIndex = Index_Create();
  if (diskIndex == NULL) {
    fprintf(stderr, "Can't create index\n");
    exit(EXIT_FAILURE);
  }
  if (!quietFlag) {
    printf("Starting index build ....\n");
  }

  int64_t startTime = Debug_GetTimeInMicrosecs();

  int err = Scan_TreeAndIndex("/", diskIndex, store, discardDups);
  if (err) {
    fprintf(stderr, "Error creating index\n");
    exit(EXIT_FAILURE);
  }
  int64_t endTime = Debug_GetTimeInMicrosecs();

  if (!quietFlag) {
    printf("\nIndex disk %s (latency %d) completed in %f seconds\n",
           diskpath, diskLatency, (endTime - startTime)/1000000.0);
  }
}

void
TestServiceBySingleWord(char *queryWord)
{
  QueryWord(queryWord, diskIndex, stdout);

  /* Do timing without printf */
  int64_t startTime = Debug_GetTimeInMicrosecs();
  QueryWord(queryWord, diskIndex, NULL);
  int64_t endTime = Debug_GetTimeInMicrosecs();

  if (!quietFlag) {
    printf("QueryWord \'%s\' took %f microseconds\n",queryWord,
	   (double)(endTime - startTime));
  }
}

void
TestServiceByFileOfWords(char *queryFile)
{
  const int maxWordSize = 64 + 1;  // A word is 64 characters plus a null byte
  FILE *file = fopen(queryFile, "r");

  if (file == NULL) {
    perror("fopen");
    fprintf(stderr, "Can't open query file %s\n",queryFile);
    exit(EXIT_FAILURE);
  }

  char line[maxWordSize];
  while (fgets(line, maxWordSize, file) != NULL) {
    int len = strlen(line);
    if (len > 0) {
	  /* Strip the \n character */
	  if (line[len-1] == '\n') line[len-1] = 0;
	  QueryWord(line,diskIndex, stdout);
    }
  }
  rewind(file);

  /* Do timing without printf */
  int64_t startTime = Debug_GetTimeInMicrosecs();
  while (fgets(line, maxWordSize, file) != NULL) {
    QueryWord(line,diskIndex, NULL);
  }
  int64_t endTime = Debug_GetTimeInMicrosecs();

  if (!quietFlag) {
    printf("QueryFile %s took %f microseconds\n",
           queryFile, (double)(endTime - startTime));
  }

  fclose(file);
}

int
QueryWord(char *word, Index *ind, FILE *file)
{
  IndexLocationList *loc = Index_RetrieveEntry(ind,word);
  if (loc == NULL) {
    if (file)
      fprintf(file, "Word %s not found\n", word);
    return 0;
  }

  while (loc) {
    if (file)
      fprintf(file,"Word %s @ %s:%d\n", word, loc->item.pathname, loc->item.offset);
    loc = loc->nextLocation;
  }

  return 1;
}

void
DumpStats(FILE *file)
{
  disksim_dumpstats(file);
  diskimg_dumpstats(file);
  Scan_dumpstats(file);
  Index_dumpstats(file);
  Pathstore_dumpstats(file);
  Fileops_dumpstats(file);
}

void
DumpUsageStats(FILE *file)
{
  struct rusage usage;
  int err = getrusage(RUSAGE_SELF, &usage);
  if (err) {
    fprintf(stderr, "Error getting resource usage\n");
    return;
  }

  fprintf(file, "Usage: %f usertime, %f systemtime, "
          "%ld voluntary ctxt switches, %ld involuntary ctxt switches\n",
	  usage.ru_utime.tv_sec + usage.ru_utime.tv_usec/1000000.0,
	  usage.ru_stime.tv_sec + usage.ru_stime.tv_usec/1000000.0,
	  usage.ru_nvcsw,  usage.ru_nivcsw);
}

void
PrintUsageAndExit(char *progname)
{
  fprintf(stderr, "Usage: %s <options> diskimagePath\n", progname);
  fprintf(stderr, "where <options> can be:\n");
  fprintf(stderr, "-q     don't print extra info\n");
  fprintf(stderr, "-l N   set simulated disk latency to N microseconds\n");
  fprintf(stderr, "-b     simulate disk latency by busy-waiting\n");
  fprintf(stderr, "-u     don't skip duplicate files when indexing\n");
  fprintf(stderr, "-w W   query index for word W\n");
  fprintf(stderr, "-f F   read query words from file F\n");
  fprintf(stderr, "-s     Go into server mode - (Assignment #4 only)\n");
  fprintf(stderr, "-d debugFlags   set the debug files in the debugFlags string\n");
  exit(EXIT_FAILURE);
}

