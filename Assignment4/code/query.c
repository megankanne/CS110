#include <stdio.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "imageaccess.h"
#include "query.h"


typedef struct packetHeader {
	unsigned int size;
} packetHdr;


static int
ConnectToImageServer(char *imageName)
{
  /*
   * Look up the address of the server.
   */
  struct sockaddr_in inaddr;
  int err = ImageAccess_Lookup(imageName, &inaddr);
  if (err < 0) {
    return -1;
  }

  /*
   * Create a socket and establish a TCP connection with the server.
   */
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)  {
    perror("opening socket");
    return -1;
  }

  err = connect(sockfd,(struct sockaddr *) &inaddr,sizeof(inaddr));
  if (err < 0) {
    perror("connect");
    close(sockfd);
    return -1;
  }

  return sockfd;
}

int
Query_WordLookup(char *imageName, char *word, char **result)
{
	int sockfd = ConnectToImageServer(imageName);
	if (sockfd < 0) {
		return -1;
	}
	/*
	* We now have an open TCP connection to the server.
	* Send query and get response.
	*/
		
	//send request to disksearch
	//contains imageName and word to search for
	
	char *linebuffer;
	int hsize = sizeof(packetHdr);
	unsigned int packetsize = hsize + strlen(imageName) + strlen(word) + 10; //3 for delimiter and terminating chars
	char outbuffer[packetsize];
	
	//build packet buffer
	//build header
	//print into outbuffer
	linebuffer = outbuffer;
	packetHdr *header = (packetHdr *)outbuffer;
	header->size = packetsize;
	snprintf(linebuffer + hsize, packetsize - hsize, "%s?%s", imageName, word);
	
	printf("packetsize %u\n", packetsize);
	printf("packet %s\n", linebuffer+hsize);
	
	//start sending until all outbuffer sent
    while (packetsize > 0) {
        int bytes =  write(sockfd, linebuffer, packetsize);
        if (bytes < 0) {
            perror("write");
            return -1;
        }
        packetsize -= bytes;
        linebuffer += bytes;
    }	

	
	//read response
	/* You need to implement this - Note the buffer size is likely wrong. */
	//buffer needs to be some initial value then realloc to size in header.
	
	//read response from disksearch
	
	char buf[16]; //initial buffer to hold header
	char *loc = buf;
	unsigned int nread = 0; //the number of bytes currently read
	//read at least size of packetHdr
	while (nread < sizeof(packetHdr)) {
		int bytes = read(sockfd, loc, 1);        
        if (bytes < 0) {
            perror("write");
			return -1;
        } 
        nread += bytes;
        loc += bytes;
    }
	//get the packetsize
	packetHdr *header_r = (packetHdr *)buf;
	unsigned int pktlen = header_r->size;
	//malloc the a buffer to this size
	char *respbuf = calloc(pktlen, 1);
	memcpy(respbuf, buf, nread);
	char *here = respbuf + nread;
	
	printf("nread: %u\n", nread);
	printf("read packet size %u\n", pktlen);
	//printf("diff %i\n", pktlen-nread);
	
	unsigned int diff = pktlen-nread;
	//keep reading for packetsize-bytes already read	
	for (unsigned int pos = 0; pos < diff; pos++) {
	    int retval = read(sockfd, here + pos, 1);
	    if (retval < 0) {
	    	perror("readr");
			return -1;
	    }
		nread += retval;
	}
	//char *payload = respbuf + sizeof(packetHdr);
	//printf("pos: %u\n", pos);
	//printf("nread: %u\n", nread);
	//printf("payload len %s\n", strlen(respbuf + sizeof(packetHdr)));
	//printf("read payload %s\n", respbuf + sizeof(packetHdr));
	
	*result = respbuf + sizeof(packetHdr);
	
	// char buf[1024*1024];
	// 	int nbytes = read(sockfd, buf, sizeof(buf));
	// 	if (nbytes > 0) {
	// 		if (nbytes > result_maxsize) {
	// 			nbytes = result_maxsize;
	// 		}
	// 	}
	// 	memcpy(result, buf, nbytes);
	close(sockfd);

	return nread;
}
