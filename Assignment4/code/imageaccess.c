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

static char *progLocation;
static char *imageDirectory;

static int numimageservers;
static struct ImageServer {
  char              *imageFile;
  struct sockaddr_in inaddr;
  pid_t              pid;
} imageservers[MAX_IMAGESERVERS];

/*
 * Initialize the image access module by telling it where the disksearch
 * program is and the directory to find the image files.  Return 0 on success,
 * -1 on error.
 */
int
ImageAccess_Init(char *diskSearchLoc, char *imageDirLoc)
{
  progLocation = diskSearchLoc;
  imageDirectory = imageDirLoc;

  return 0;
}

/*
 * Start a disksearch server and retrieve its its IP address and port number.
 * Return non-negative on success, -1 on error.
 */
int
ImageAccess_Start(char *imageFile)
{
  if (progLocation == NULL)
    return -1;  /* Not initialized */

  if (numimageservers >= MAX_IMAGESERVERS)
    return -1; /* Full */

  /*
   * Create a pipe to pass to the disksearch server so it can tell us when it
   * is ready and inform us of the IP address and port number to use.
   */
  int pipefds[2];
  int err = pipe(pipefds);
  if (err < 0) {
    perror("pipe");
    return -1;
  }

  /*
   * Create a new process (thread/address space) to run the disksearch server.
   */
  pid_t pid = fork ();
  if (pid == (pid_t) 0)    {
    /* Child side.  */
    const char *new_argv[5];
    char ackFd[32];
    char imagePath[strlen(imageDirectory) + sizeof('/') +
      strlen(imageFile) + sizeof('\000')];

    sprintf(imagePath, "%s/%s", imageDirectory, imageFile);
    snprintf(ackFd, sizeof(ackFd),"%d", pipefds[1]);
    new_argv[0] = progLocation;
    new_argv[1] =  "-s";
    new_argv[2] = ackFd;
    new_argv[3] = imagePath;
    new_argv[4] = NULL;

    close(pipefds[0]); /* The disksearch process doesn't need this end of the pipe */
    /* Run the program. */
    (void) execv(progLocation, (char *const *) new_argv);

    perror("execv");  /* We won't get here unless exec failed. */
    exit (-1);
  }  else if (pid < (pid_t) 0) {
    /* The fork failed.  */
    close(pipefds[0]);
    close(pipefds[1]);
    return -1;
  } else {
    /* Parent */
    close (pipefds[1]);  /* Parent doesn't need the write end to the pipe. */

    err = read(pipefds[0], &imageservers[numimageservers].inaddr, sizeof(struct sockaddr_in));
    if (err < 0) {
      perror("read");
    }
    close (pipefds[0]);
    if (err != sizeof(struct sockaddr_in)) {
      return -1;
    }
    imageservers[numimageservers].imageFile = strdup(imageFile);
    imageservers[numimageservers].pid = pid;
    numimageservers++;
  }

  return numimageservers-1;
}

/*
 * Look up the address of the server for the specified image files.  Return 0
 * on success, -1 on error.
 */
int
ImageAccess_Lookup(char *imageFile, struct sockaddr_in *inaddrPtr)
{
  for (int i = 0; i < numimageservers; i++) {
    if (strcmp(imageservers[i].imageFile,imageFile) == 0) {
      *inaddrPtr = imageservers[i].inaddr;
      return 0;
    }
  }
  return -1;
}
