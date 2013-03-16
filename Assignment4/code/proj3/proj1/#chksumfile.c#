#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "diskimg.h"

#include "unixfilesystem.h"

#include "inode.h"
#include "file.h"
#include "directory.h"
#include "pathname.h"

#include "chksumfile.h"

/*
 * We use SHA1 (a secure hash function) to compute the checksum.
 * 
 */
#if defined(__APPLE__)
#  define COMMON_DIGEST_FOR_OPENSSL
#  include <CommonCrypto/CommonDigest.h>
#else
#  include <openssl/sha.h>
#endif

/*
 * Compute the checksum of a inumber. Assumes chksum arguments points to a
 * CHKSUMFILE_SIZE byte array. Returns the length of the checksum, or -1 if
 * it encounters an error.
 */
int
chksumfile_byinumber(struct unixfilesystem *fs, int inumber, void *chksum)
{
  SHA_CTX shactx;
  if (!SHA1_Init(&shactx)) {
    // An error occurred initializing the SHA1 context.
    return -1;
  }

  struct inode in;
  int err = inode_iget(fs, inumber, &in);
  if (err < 0) {
    return err;
  }

  if (!(in.i_mode & IALLOC)) {
    // The inode isn't allocated, so we can't hash it.
    return -1;
  }

  int size = inode_getsize(&in);

  for (int offset = 0; offset < size; offset += DISKIMG_SECTOR_SIZE) {
    char buf[DISKIMG_SECTOR_SIZE];
    int bno = offset/DISKIMG_SECTOR_SIZE;

    int bytesMoved = file_getblock(fs, inumber, bno, buf);
    if (bytesMoved < 0)
      return -1;

    if (!SHA1_Update(&shactx, buf, bytesMoved))
      return -1;
  }

  if (!SHA1_Final(chksum, &shactx))
    return -1;

  return SHA_DIGEST_LENGTH;
}

/*
 * Compute the checksum of the specified pathname.  Assumes chksum points to a
 * CHKSUMFILE_SIZE byte array. Returns the length of the checksum or -1 if
 * it encounters an error.
 */

int
chksumfile_bypathname(struct unixfilesystem *fs, const char *pathname, void *chksum)
{
  int inumber = pathname_lookup(fs, pathname);
  if (inumber < 0) {
    return inumber;
  }

  return chksumfile_byinumber(fs, inumber, chksum);
}

/*
 * Convert a checksum into a string that can be printed.  Assumes
 * that outstring is CHKSUMFILE_STRINGSIZE in size.
 */
void 
chksumfile_cvt2string(void *chksum, char *outstring)
{
  uint8_t *c = (uint8_t *) chksum;

  for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
    sprintf(outstring + 2 * i, "%02x", c[i]);
  }
}

/*
 * Compares two checksums, returning 1 if they're the same and 0 otherwise.
 */
int
chksumfile_compare(void *chksum1, void *chksum2)
{
  uint8_t *c1 = (uint8_t *) chksum1;
  uint8_t *c2 = (uint8_t *) chksum2;

  for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
    if (c1[i] != c2[i]) return 0;
  }
  return 1;
}

