/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#ifndef _WIN32

/* UNIX includes */
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <ctype.h>

#define O_BINARY 0

#define HAVE_VSNPRINTF
#define HAVE_FSEEKO
#define HAVE_FTRUNCATE

#else /*_WIN32*/

#if !defined(AFX_STDAFX_H__1CB7B33E_42BF_4A98_B814_4198EA8ACC58__INCLUDED_)
#define AFX_STDAFX_H__1CB7B33E_42BF_4A98_B814_4198EA8ACC58__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define HAVE_WINDOWS_CDROM      // enable CD-ROM access under Windows
#define HAVE_CHSIZE


// Insert your headers here
# define WIN32_LEAN_AND_MEAN        // Exclude rarely-used stuff from Windows headers

#define _CRT_SECURE_NO_WARNINGS     // disable VC++ libc complaints here

#include "../app/targetver.h"

#include <windows.h>
#include <atlstr.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <io.h>
#include <fcntl.h>

#ifdef HAVE_WINDOWS_CDROM
# include <winioctl.h>
#endif

#ifndef _SSIZE_T_DEFINED
typedef unsigned int ssize_t;
#define _SSIZE_T_DEFINED
#endif

#define HAVE__VSNPRINTF
#define strcasecmp stricmp


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__1CB7B33E_42BF_4A98_B814_4198EA8ACC58__INCLUDED_)
#endif /*_WIN32*/
