/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Standard stuff.
 */
#ifndef __FADDEN_STD__
#define __FADDEN_STD__

#define NELEM(x) ((int) (sizeof(x) / sizeof(x[0])))

#define nil NULL

// Windows equivalents
#define strcasecmp      stricmp
#define strncasecmp     strnicmp

#endif /*__FADDEN_STD__*/