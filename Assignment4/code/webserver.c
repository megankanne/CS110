#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>

#include "imageaccess.h"
#include "debug.h"
#include "http.h"

static int ReadConfigFile(char *pathname);
static void handleSIGCHLD();
static int InitializeBackends(void);
static int SetupListenSocket(void);
static void HandleConnections(int listenfd);
static void PrintUsageAndExit(char *progname);

int quietFlag = 0;
/* Read from config file */
unsigned short portno = 0;  /* Default is to dynamicaly allocate port */
char *imgfiledir = ".";  /* Where to find the disk images */
char *disksearch = "./disksearch";  /* Pathname of disksearch program */
static int numImgFiles = 0;
static char *imgFileNames[MAX_IMAGESERVERS];


int
main(int argc, char *argv[])
{
  int opt;
  while ((opt = getopt(argc, argv, "ql:d:uw:f:bc:s:")) != -1) {
    switch (opt) {
    case 'q':
      quietFlag = 1;
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

  char *configfile = argv[optind];
  int err = ReadConfigFile(configfile);
  if (err < 0) {
    exit(EXIT_FAILURE);
  }

  /*
   * Since we create a process (i.e. fork) we need to clean up its state when
   * we exit.  We do this by having the OS send us a signal when something
   * finishes.
   */
  signal(SIGCHLD, handleSIGCHLD);

  err = InitializeBackends();
  if (err < 0) {
    exit(EXIT_FAILURE);
  }

  int listenfd = SetupListenSocket();
  if(listenfd < 0){
    exit(EXIT_FAILURE);
  }
  HandleConnections(listenfd);

  return 0; /* Never reached. */
}

/*
 * Read the config file.
 */
static int
ReadConfigFile(char *pathname)
{

  FILE *f = fopen(pathname, "r");
  if (f == (FILE *) NULL) {
    perror("config file");
    fprintf(stderr, "Can't open %s\n", pathname);
    return -1;
  }

  char line[1025];
  while (fgets(line, 1024, f) != NULL) {
    if (line[0] == '#')
      continue;  /* Comment lines start with #. */

    int portNumber;
    int n = sscanf(line, "portno: %d\n", &portNumber);
    if (n == 1) {
      portno = (short) portNumber;
      continue;
    }
    char tmpstring[sizeof(line)];
    n = sscanf(line, "imgfiledir: %s\n", tmpstring);
    if (n == 1) {
      imgfiledir = strdup(tmpstring);
      continue;
    }
    n = sscanf(line, "disksearch: %s\n", tmpstring);
    if (n == 1) {
      disksearch = strdup(tmpstring);
      continue;
    }
    n = sscanf(line, "imgfile: %s\n", tmpstring);
    if (n == 1) {
      if (numImgFiles < MAX_IMAGESERVERS) {
        imgFileNames[numImgFiles] = strdup(tmpstring);
        numImgFiles++;
        continue;
      } else {
        fprintf(stderr, "Too many image files specified\n");
        fclose(f);
        return -1;
      }
    }
    if (line[0] != '\n') { /* Don't report blank lines */
      fprintf(stderr, "Skipping unknown option line in config file (%s)\n", line);
    }
  }
  fclose(f);
  return 0;
}

/*
 * catch the signal sent by OS when a process finishes
 */
static void
handleSIGCHLD(void)
{
  int stat;
  int pid;

  /* Wait for all our children to finish. */
  while((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
    DPRINTF('p', ("Pid %d returned 0x%x\n", pid, stat));
  }
}

/*
 * Start up access to the disk image servers.
 */
static int
InitializeBackends(void)
{
  int err = ImageAccess_Init(disksearch, imgfiledir);
  if (err < 0) {
    return -1;
  }
  for (int i = 0; i < numImgFiles; i++) {
    err = ImageAccess_Start(imgFileNames[i]);
    if (err < 0) {
      return -1;
    }
  }
  return 0;
}

/*
 * Set up the socket for listening to connections and print out the websever
 * URL for the browser to load.
 */
int SetupListenSocket(void)
{
  /*
   * Create a TCP socket to listen for connection on.
   */
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    perror("socket");
    return -1;
  }

  /*
   * Bind the socket to the address/port we will publish.
   */
  struct sockaddr_in servaddr;
  memset(&servaddr,0,sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr= htonl(INADDR_ANY);
  servaddr.sin_port=htons(portno);
  int err = bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
  if (err < 0) {
    perror("bind");
    exit(1);
  }

  /*
   * Setup the socket/port to listen from incomming connections.
   */
  err = listen(listenfd, 4);
  if (err < 0) {
    perror("listen");
    return -1;
  }

  char hostname[1024];
  err = gethostname(hostname, 1024);
  if (err < 0) {
    perror("gethostname");
    return -1;
  }

  socklen_t len = sizeof(servaddr);
  err = getsockname(listenfd,  (struct sockaddr *) &servaddr, &len);
  if (err < 0) {
      perror("getsockname");
      return -1;
  }
  portno = ntohs(servaddr.sin_port);
  printf("Webserver URL: http://%s:%d\n", hostname, (int)portno);

  return listenfd;
}

/*
 * Accept connections and fork off process to handle the connection.
 */
void HandleConnections(int listenfd)
{
  int connfd;
  int pid;

  printf("\n['ctrl-c' to shut down the server]\n");

  /* main loop */
  for(;;) {
    /* Accept the connection. */
    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);
    connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);

    char *n = inet_ntoa(cliaddr.sin_addr);
    DPRINTF('c', ("Got connection from %s port %d\n", n, ntohs(cliaddr.sin_port)));

    /* Fork to create private address space to process this connection. */
    pid = fork();
    if (pid < 0) {
      perror("fork");
      exit(1);
    }

    if (pid == 0){
      /*
       * Child Process. It doesn't need the socket we are listening on so we
       * close it.
       */
      close(listenfd);
      DPRINTF('p', ("Child %d starting\n", getpid()));
      Http_ProcessConnection(connfd);
      DPRINTF('p', ("Child %d exiting\n", getpid()));
      exit(0);
    }
    else{
      /* Parent process - We don't need the connected socket so we close it. */
      close(connfd);
    }
  }
}

/*
 * Print the program command line arguments and exit.
 */
void
PrintUsageAndExit(char *progname)
{
  fprintf(stderr, "Usage: %s <options> configFile\n", progname);
  fprintf(stderr, "where <options> can be:\n");
  fprintf(stderr, "-q     don't print extra info\n");
  fprintf(stderr, "-d debugFlags   set the debug files in the debugFlags string\n");
  exit(EXIT_FAILURE);
}
