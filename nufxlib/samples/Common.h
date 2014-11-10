/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING.LIB.
 *
 * Common functions for NuLib tests.
 */
#ifndef __Common__
#define __Common__

#include "SysDefs.h"        /* might as well draft off the autoconf */
#include "NufxLib.h"

#ifdef USE_DMALLOC
# include "dmalloc.h"
#endif

#define nil NULL    /* this is seriously habit-forming */

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

#ifndef __cplusplus
 #define false  0
 #define true   (!false)
#endif


#ifdef FOPEN_WANTS_B
# define kNuFileOpenReadOnly        "rb"
# define kNuFileOpenReadWrite       "r+b"
# define kNuFileOpenWriteTrunc      "wb"
# define kNuFileOpenReadWriteCreat  "w+b"
#else
# define kNuFileOpenReadOnly        "r"
# define kNuFileOpenReadWrite       "r+"
# define kNuFileOpenWriteTrunc      "w"
# define kNuFileOpenReadWriteCreat  "w+"
#endif


/*
 * Figure out what path separator to use.
 *
 * NOTE: recent versions of Win32 will also accept '/'.
 */

#ifdef MSDOS
# define PATH_SEP   '\\'
#endif

#ifdef WIN32
# define PATH_SEP   '\\'
#endif

#ifdef MACOS
# define PATH_SEP   ':'
#endif

#if defined(APW) || defined(__ORCAC__)
# define PATH_SEP   ':'
#endif

#ifndef PATH_SEP
# define PATH_SEP   '/'
#endif

#endif /*__Common__*/
