#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include "debug.h"

#include "http.h"
#include "query.h"

/*
 * Process a HTTP protocol connection.
 */

static int HttpReadLine(int sock, char *linebuffer, int maxsize);
static int HttpReadNBytes(int sock, char *buffer, int nbytes);
static int HttpWriteLine(int sock, char *linebuffer, int size);
static int SendHtmlReply(int connfd, char *status, char *html);
static int SendTextReply(int connfd, char *status, char *text);

int sendHttp = 1;

/*
 * Process an HTTP GET request.
 */
static void
HandleGetRequest(int connfd, char *path, char *version)
{
  char *replyMessageFormat =
    "<html>\n"
    "<h1>Welcome to the world's lamest webserver</h1>\n"
    "<body>\n"
    "<h2>Got a GET request</h2>\n"
    "<h3>Path argument was: %s</h3>\n"
    "<h3>Version was: HTTP/%s</h3>\n"
    "Attempt to query file simple.img for word fooA returns:\n%s\n"
    "</body>\n"
    "</html>";

  /*
   * Make this a little more of a test by attempting to send a query to the
   * backend.  Note that qresult size assumes the diskresult is going to be
   * small. You will want to change this.
   */

  char qresult[1024]; /* Note this is almost certainly wrong size. */
  int nbytes = Query_WordLookup("simple.img", "fooA", qresult, sizeof(qresult)-1);

  if (nbytes < 0) {
    sprintf(qresult, "ERROR\n");
  } else {
    qresult[nbytes] = 0;
  }

  char buffer[strlen(replyMessageFormat) + strlen(path) +
              strlen(version) + strlen(qresult) + 10];
  sprintf(buffer, replyMessageFormat, path, version, qresult);

  if (sendHttp) {
    SendHtmlReply(connfd, "HTTP/1.0 200 OK", buffer);
  } else {
    /* This is mainly here to stop compiler from complaining about
     * SendTextReply never called. */
    SendTextReply(connfd, "HTTP/1.0 200 OK", buffer);
  }
}

/*
 * Process an HTTP POST request.
 */
static void
HandlePostRequest(int connfd, char *path, char *version, char *contents)
{
  char *replyMessageFormat =
    "<html>\n"
    "<body>\n"
    "<h2>Got a POST request</h2>\n"
    "<h3>Path argument was: %s</h3>\n"
    "<h3>Version was: HTTP/%s</h3>\n"
    "<h3>Contents was: %s</h3>\n"
    "</body>\n"
    "</html>";

  char buffer[strlen(replyMessageFormat) + strlen(path) +
              strlen(version) + strlen(contents) + 10];
  sprintf(buffer, replyMessageFormat, path, version, contents);

  SendHtmlReply(connfd, "HTTP/1.0 200 OK", buffer);
}

/*
 * Process a HTTP connection.
 */
int
Http_ProcessConnection(int connfd)
{
  /*
   * The first line should the HTTP request command.
   */
  char requestString[4096];
  int len = HttpReadLine(connfd, requestString, sizeof(requestString));
  if (len < 0)
    return -1;

  DPRINTF('h', ("Got HTTP request \"%s\"\n", requestString));

  /*
   * Process the headers until we get a blank line.  Currently the only header
   * we care about is the Content length header.
   */
  int contentLength = 0;
  do {
    char line[4096];
    len = HttpReadLine(connfd, line, sizeof(line));
    if (len < 0) break;

    DPRINTF('h',("Header Line:\"%s\"\n",line));

    int clen;
    int n = sscanf(line, "Content-Length: %d", &clen);
    if (n == 1) {
      contentLength = clen;
      DPRINTF('h',("Found length of %d\n", clen));
    }
  } while (len > 0);  /* A blank line has len==0. */

  char *contents = NULL;
  if (contentLength > 0) {
    contents = malloc(contentLength+1);

    /* Read the contents of the request. */
    HttpReadNBytes(connfd, contents, contentLength);
    contents[contentLength] = 0;
    DPRINTF('h',("Read request bytes %s\n",contents));
  }

  /* We got a request; now handle it. */
  char path[sizeof(requestString)];
  char version[sizeof(requestString)];

  /* Check to see if its a GET request. */
  int n = sscanf(requestString, "GET %s HTTP/%s", path, version);
  if (n == 2) {
    DPRINTF('h',("Found GET request of path %s and version %s\n", path, version));
    HandleGetRequest(connfd, path, version);
  }
  else {
    n = sscanf(requestString, "POST %s HTTP/%s", path, version);
    if  (n == 2) {
      DPRINTF('h',("Found POST request of path %s and version %s\n", path, version));
      HandlePostRequest(connfd, path, version, contents);
    }
    else {
      DPRINTF('h',("Unknown request\n"));
      char ebuffer[strlen(requestString)+200];
      sprintf(ebuffer, "<html>\n<h1>Error</h1>\n<body>\n%s\n</body><html>\n", requestString);
      SendHtmlReply(connfd, "HTTP/1.0 501 Not Implemented", ebuffer);
    }
  }

  free(contents);
  close(connfd);
  return 0;
}


/*
 * Write a HTTP protocol line the specifed socket. Return -1 on error.
 */
static int
HttpWriteLine(int sock, char *linebuffer, int size)
{
    size += strlen("\r\n");

    char outbuffer[size + 1];

    snprintf(outbuffer, sizeof(outbuffer), "%s\r\n", linebuffer);
    linebuffer = outbuffer;

    while (size > 0) {
        int bytes =  write(sock, linebuffer, size);
        
        if (bytes < 0) {
            perror("write");
            return -1;
        }
        
        size -= bytes;
        linebuffer += bytes;
    }

    return 0;
}


/*
 * Read a HTTP protocol line the specifed sock. Return -1 on error, otherwise
 * number of bytes in the line.
 * Read and discard if line is longer than buffer.
 */
static int
HttpReadLine(int sock, char *linebuffer, int maxsize)
{
  char lastch = -1;
  int retval = 1;

  int pos;
  for (pos = 0; true; pos++) {
    char ch;
    retval = read(sock, &ch, 1);
    if (retval < 0) {
      perror("read");
      break;
    }
    if (retval == 0) {
      break;
    }
    if ((ch == '\n') && (lastch == '\r')) {
      pos--;  // Strip \r from buffer;
      break;
    }
    if (pos < maxsize-1) {
      linebuffer[pos] = ch;
    }
    lastch = ch;
  }

  linebuffer[pos] = 0;

  return (retval != 1) ? -1 : pos;
}

/*
 * Read n bytes from the socket. Return -1 on error, n otherwise.
 */
static int
HttpReadNBytes(int sock, char *buffer, int n)
{
  for (int pos = 0; pos < n; pos++) {
    int retval = read(sock, buffer + pos, 1);
    if (retval < 0) {
      perror("readr");
      return -1;
    }
  }
  return n;
}


/*
 * Send an HTTP reply back to the client formatted as an HTTP message.
 */
static int
SendHtmlReply(int connfd, char *status, char *html)
{
  int err;
  char clbuf[sizeof("Content-length: XXXXXXXXX")];
  snprintf(clbuf, sizeof(clbuf), "Content-length: %d", (int)strlen(html));

#define _WL(string)                                             \
  err = HttpWriteLine(connfd, (string), strlen(string));        \
  if (err < 0) return err;
  
  /*
   * Output:
   * status line
   * Headers
   * BlankLine
   * html
   * BlankLine
   */
  _WL(status);
  _WL("Connection: close");
  _WL("Content-Type: text/html");
  _WL(clbuf);
  _WL("");
  _WL(html);
  _WL("");

#undef _WL

  return 0;
}

/*
 * Send a HTTP reply back to the client formatted as plain text
 */
static int
SendTextReply(int connfd, char *status, char *text)
{
  int err;
  char clbuf[sizeof("Content-length: XXXXXXXXX")];
  snprintf(clbuf, sizeof(clbuf), "Content-length: %d", (int)strlen(text));
  
#define _WL(string)                                                     \
  err = HttpWriteLine(connfd, (string), strlen(string));                \
  if (err < 0) return err;
  
  /*
   * Output:
   * status line
   * Headers
   * BlankLine
   * html
   * BlankLine
   */
  _WL(status);
  _WL("Connection: close");
  _WL("Content-Type: text/plain");
  _WL(clbuf);
  _WL("");
  _WL(text);
  _WL("");

#undef _WL

  return 0;
}
