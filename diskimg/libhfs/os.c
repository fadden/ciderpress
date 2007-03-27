/*
 * libhfs - library for reading and writing Macintosh HFS volumes
 * Copyright (C) 1996-1998 Robert Leslie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# ifdef HAVE_FCNTL_H
#  include <fcntl.h>
# else
int open(const char *, int, ...);
int fcntl(int, int, ...);
# endif

# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# else
#ifndef _WIN32
int close(int);
off_t lseek(int, off_t, int);
ssize_t read(int, void *, size_t);
ssize_t write(int, const char *, size_t);
int stat(const char *, struct stat *);
int fstat(int, struct stat *);
#endif
# endif

# include <errno.h>
# include <sys/stat.h>
# include <stdlib.h>
# include <stdio.h>     /* debug */

# include "libhfs.h"
# include "os.h"

typedef struct cp_private {
  oscallback func;          /* function to call */
  void* cookie;             /* magic cookie to pass in */
  long cur_block;           /* current seek offset */
} cp_private;

/*
 * NAME:	os->callback_open()
 * DESCRIPTION:	open and lock a new descriptor from the given path and mode
 */
int os_callback_open(void **priv, oscallback func, void* cookie)
{
  cp_private* mypriv;

  mypriv = malloc(sizeof(*mypriv));
  mypriv->func = func;
  mypriv->cookie = cookie;
  mypriv->cur_block = 0;

  *priv = mypriv;
  //fprintf(stderr, "ALLOC %p->%p\n", priv, *priv);

  return 0;
}

/*
 * NAME:	os->close()
 * DESCRIPTION:	close an open descriptor
 */
int os_close(void **priv)
{
  //fprintf(stderr, "FREEING %p->%p\n", priv, *priv);
  free(*priv);
  *priv = 0;

  return 0;
}

#ifdef CP_NOT_USED
/*
 * NAME:	os->same()
 * DESCRIPTION:	return 1 iff path is same as the open descriptor
 */
int os_same(void **priv, const char *path)
{
  return 0;

  int fd = (int) *priv;
  struct stat fdev, dev;

  if (fstat(fd, &fdev) == -1 ||
      stat(path, &dev) == -1)
    ERROR(errno, "can't get path information");

  return fdev.st_dev == dev.st_dev &&
         fdev.st_ino == dev.st_ino;

fail:
  return -1;
}
#endif

/*
 * NAME:	os->seek()
 * DESCRIPTION:	set a descriptor's seek pointer (offset in blocks)
 */
unsigned long os_seek(void **priv, unsigned long offset)
{
  cp_private* mypriv = (cp_private*) *priv;
  unsigned long result;

  if (offset == (unsigned long) -1) {
    result = (*mypriv->func)(mypriv->cookie, HFS_CB_VOLSIZE, 0, 0);
  } else {
    result = (*mypriv->func)(mypriv->cookie, HFS_CB_SEEK, offset, 0);
    if (result != -1)
        mypriv->cur_block = offset;
  }

  return result;
}

/*
 * NAME:	os->read()
 * DESCRIPTION:	read blocks from an open descriptor
 */
unsigned long os_read(void **priv, void *buf, unsigned long len)
{
  cp_private* mypriv = (cp_private*) *priv;
  unsigned long result;
  unsigned long success = 0;

  while (len--) {
      result = (*mypriv->func)(mypriv->cookie, HFS_CB_READ,
                                mypriv->cur_block, buf);
      if (result == -1)
        break;

      mypriv->cur_block++;
      buf = ((unsigned char*) buf) + HFS_BLOCKSZ;
      success++;
  }

  return success;
}

/*
 * NAME:	os->write()
 * DESCRIPTION:	write blocks to an open descriptor
 */
unsigned long os_write(void **priv, const void *buf, unsigned long len)
{
  cp_private* mypriv = (cp_private*) *priv;
  unsigned long result;
  unsigned long success = 0;

  while (len--) {
      result = (*mypriv->func)(mypriv->cookie, HFS_CB_WRITE,
                                mypriv->cur_block, (void*)buf);
      if (result == -1)
        break;

      mypriv->cur_block++;
      buf = ((unsigned char*) buf) + HFS_BLOCKSZ;
      success++;
  }

  return success;
}
