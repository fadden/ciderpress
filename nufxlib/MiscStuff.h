/*
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Misc stuff (shared between nufxlib and nulib2).  This is a collection
 * of miscellaneous types and macros that I find generally useful.
 */
#ifndef NUFXLIB_MISCSTUFF_H
#define NUFXLIB_MISCSTUFF_H

#define VALGRIND        /* assume we're using it */

#include "SysDefs.h"

/*
 * Use our versions of functions if they don't exist locally.
 */
#ifndef HAVE_STRERROR
#define strerror Nu_strerror
const char* Nu_strerror(int errnum);
#endif
#ifndef HAVE_MEMMOVE
#define memmove Nu_memmove
void* Nu_memmove(void *dest, const void *src, size_t n);
#endif
#ifndef HAVE_STRTOUL
#define strtoul Nu_strtoul
unsigned long Nu_strtoul(const char *nptr, char **endptr, int base);
#endif
#ifndef HAVE_STRCASECMP
#define strcasecmp Nu_strcasecmp
int Nu_strcasecmp(const char *s1, const char *s2);
#endif
#ifndef HAVE_STRNCASECMP
#define strncasecmp Nu_strncasecmp
int Nu_strncasecmp(const char *s1, const char *s2, size_t n);
#endif


/*
 * Misc types.
 */

typedef unsigned char Boolean;
#define false   (0)
#define true    (!false)


/*
 * Handy macros.
 */

/* compute #of elements in a static array */
#define NELEM(x)    (sizeof(x) / sizeof((x)[0]))

/* convert single hex digit char to number */
#define HexDigit(x) ( !isxdigit((int)(x)) ? -1 : \
            (x) <= '9' ? (x) - '0' : toupper(x) +10 - 'A' )

/* convert number from 0-15 to hex digit */
#define HexConv(x)  ( ((unsigned int)(x)) <= 15 ? \
            ( (x) <= 9 ? (x) + '0' : (x) -10 + 'A') : -1 )


/*
 * Debug stuff.
 */

/*
 * Redefine this if you want assertions to do something other than default.
 * Changing the definition of assert is tough, because assert.h redefines
 * it every time it's included.  On a Solaris 2.7 system I was using, gcc
 * pulled assert.h in with some of the system headers, and their definition
 * resulted in corrupted core dumps.
 */
#define Assert assert

#if defined(DEBUG_VERBOSE)
 /* quick debug printf macro */
 #define DBUG(args)             printf args
#else
 #define DBUG(args)             ((void)0)
#endif


#if defined(NDEBUG)
 #define DebugFill(addr, len)   ((void)0)

 #define DebugAbort()           ((void)0)

#else
 /* when debugging, fill Malloc blocks with junk, unless we're using Purify */
 #if !defined(PURIFY) && !defined(VALGRIND)
  #define DebugFill(addr, len)  memset(addr, 0xa3, len)
 #else
  #define DebugFill(addr, len)  ((void)0)
 #endif

 #define DebugAbort()           abort()
#endif

#define kInvalidPtr     ((void*)0xa3a3a3a3)

#endif /*NUFXLIB_MISCSTUFF_H*/
