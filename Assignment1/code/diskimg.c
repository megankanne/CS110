#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "diskimg.h"

/* Open a disk image for I/O. Returns an open file descriptor, or -1 if
 * unsuccessful.  */
int
diskimg_open (char *pathname, int readOnly)
{
  return open(pathname, readOnly ? O_RDONLY : O_RDWR);
}

/*
 * Return the size of the disk imgage in bytes, or -1 if unsuccessful.
 */
int
diskimg_getsize(int fd)
{
  return lseek(fd, 0, SEEK_END);
}

/*
 * Read the specified sector from the disk.  Returns the number of bytes read,
 * or -1 on error.
 */
int
diskimg_readsector(int fd, int sectorNum,  void *buf)
{
  /* NOTE: This is not thread safe. */
  if (lseek(fd, sectorNum * DISKIMG_SECTOR_SIZE, SEEK_SET) == (off_t) -1) {
    return -1;
  }

  return read(fd, buf, DISKIMG_SECTOR_SIZE);
}

/*
 * Write the specified sector from the disk.  Returns the number of bytes
 * written, or -1 on error.
 */
int diskimg_writesector(int fd, int sectorNum,  void *buf)
{
  /* NOTE: This is not thread safe. */
  if (lseek(fd, sectorNum * DISKIMG_SECTOR_SIZE, SEEK_SET) == (off_t) -1) {
    return -1;
  }

  return write(fd, buf, DISKIMG_SECTOR_SIZE);
}

/*
 * Clean up from a previous diskimg_open() call.  Returns 0 on success, or -1 on
 * error.
 */
int
diskimg_close(int fd)
{
  return close(fd);
}
