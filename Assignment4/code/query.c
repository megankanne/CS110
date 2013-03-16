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
	unsigned int more;
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
Query_WordLookup(char *imageName, char *word, char *result, int result_maxsize)
{
	int sockfd = ConnectToImageServer(imageName);
	if (sockfd < 0) {
		return -1;
	}
	/*
	* We now have an open TCP connection to the server.
	* Send query and get response.
	*/
	
	//TO DO
	
	// for (int pos = 0; pos < n; pos++) {
	// 	    int retval = read(sock, buffer + pos, 1);
	// 	    if (retval < 0) {
	// 	      perror("readr");
	// 	      return -1;
	// 	    }
	// 	  }
	// 	  return n;
	
	//send request (write?) to disksearch
	//contains imageName and word to search for
	//simple.img, alpha word
	
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
	header->more = 0;
	snprintf(linebuffer + hsize, packetsize - hsize, "%s?%s", imageName, word);
	
	printf("packetsize %u\n", packetsize);
	printf("packet %s\n", linebuffer+hsize);
	//printf("from header %u\n", header->size);
	//printf("from array %u\n", *linebuffer);
	
	
 	
	//start sending
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
	
	char buf[1024*1024];
	int nbytes = read(sockfd, buf, sizeof(buf));
	if (nbytes > 0) {
		if (nbytes > result_maxsize) {
			nbytes = result_maxsize;
		}
	}
	memcpy(result, buf, nbytes);
	close(sockfd);

	return nbytes;
}
