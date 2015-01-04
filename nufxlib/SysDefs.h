/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * External type definitions and function prototypes.
 */
#ifndef NUFXLIB_SYSDEFS_H
#define NUFXLIB_SYSDEFS_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef DEBUG_VERBOSE
# define DEBUG_MSGS
#endif

/* these should exist everywhere */
#include <stdarg.h>
#include <string.h>
#include <memory.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

/* basic Win32 stuff -- info-zip has much more complete defs */
#if defined(_WIN32) || defined(MSDOS)
# define WINDOWS_LIKE

# ifndef HAVE_CONFIG_H
#  define HAVE_FCNTL_H
#  define HAVE_MALLOC_H
#  define HAVE_STDLIB_H
#  define HAVE_SYS_STAT_H
#  undef HAVE_SYS_TIME_H
#  define HAVE_SYS_TYPES_H
#  undef HAVE_UNISTD_H
#  undef HAVE_UTIME_H
#  define HAVE_SYS_UTIME_H
#  define HAVE_WINDOWS_H
#  define HAVE_FDOPEN
#  undef HAVE_FTRUNCATE
#  define HAVE_MEMMOVE
#  undef HAVE_MKSTEMP
#  define HAVE_MKTIME
#  define HAVE_SNPRINTF
#  undef HAVE_STRCASECMP
#  undef HAVE_STRNCASECMP
#  define HAVE_STRERROR
#  define HAVE_STRTOUL
#  define HAVE_VSNPRINTF
#  define SNPRINTF_DECLARED
#  define VSNPRINTF_DECLARED
#  define SPRINTF_RETURNS_INT
#  define inline /*Visual C++6.0 can't inline ".c" files*/
#  define mode_t int
#  define ENABLE_SQ
#  define ENABLE_LZW
#  define ENABLE_LZC
/*#  define ENABLE_DEFLATE*/
/*#  define ENABLE_BZIP2*/
# endif

# include <io.h>
# include <direct.h>
# define FOPEN_WANTS_B
# define HAVE_CHSIZE
# define snprintf _snprintf
# define vsnprintf _vsnprintf

#endif

#ifdef HAVE_MALLOC_H
# include <malloc.h>
#endif
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_UTIME_H
# include <utime.h>
#endif
#ifdef HAVE_SYS_UTIME_H
# include <sys/utime.h>
#endif

#if defined(WINDOWS_LIKE)
# ifndef F_OK
#  define F_OK 0            /* was 02 in <= v1.1.0 */
# endif
#endif

#if defined(__APPLE__) && defined(__MACH__)     /* OS X */
# define MAC_LIKE
# define UNIX_LIKE
#endif

#if defined(__unix__) || defined(__unix) || defined(__BEOS__) || \
    defined(__hpux) || defined(_AIX)
# define UNIX_LIKE      /* standardize */
#endif

#if defined(UNIX_LIKE)
# ifdef USE_REENTRANT_CALLS
#  define _REENTRANT    /* Solaris 2.x convention */
# endif
#endif

/* not currently using filesystem resource forks */
//#if defined(__ORCAC__) || defined(MAC_LIKE)
//# define HAS_RESOURCE_FORKS
//#endif

/* __FUNCTION__ was missing from BeOS __MWERKS__, and might be gcc-only */
#ifdef __GNUC__
# define HAS__FUNCTION__
#endif

#if defined(__linux__)
# define HAS_MALLOC_CHECK_
#endif

#endif /*NUFXLIB_SYSDEFS_H*/
