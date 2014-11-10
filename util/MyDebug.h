/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * My debug stuff.
 */
#ifndef UTIL_MYDEBUG_H
#define UTIL_MYDEBUG_H

//#define _DEBUG_LOG        /* set this to force logging in all builds */

#ifndef _DEBUG
//# define _DEBUG_LOG       /* define this to use logging for !_DEBUG */
#endif

#if defined(_DEBUG_LOG)
#include <stdio.h>
extern FILE* gLog;
extern int gPid;
#define WMSG0(fmt) \
    { fprintf(gLog, "%05u ", gPid); fprintf(gLog, fmt); }
#define WMSG1(fmt, arg0) \
    { fprintf(gLog, "%05u ", gPid); fprintf(gLog, fmt, arg0); }
#define WMSG2(fmt, arg0, arg1) \
    { fprintf(gLog, "%05u ", gPid); fprintf(gLog, fmt, arg0, arg1); }
#define WMSG3(fmt, arg0, arg1, arg2) \
    { fprintf(gLog, "%05u ", gPid); fprintf(gLog, fmt, arg0, arg1, arg2); }
#define WMSG4(fmt, arg0, arg1, arg2, arg3) \
    { fprintf(gLog, "%05u ", gPid); fprintf(gLog, fmt, arg0, arg1, arg2, arg3); }
#define WMSG5(fmt, arg0, arg1, arg2, arg3, arg4) \
    { fprintf(gLog, "%05u ", gPid); fprintf(gLog, fmt, arg0, arg1, arg2, arg3, \
            arg4); }

#else
/* can use TRACE0, TRACE1, etc to avoid header and '\n' */
#define WMSG0(fmt) _RPTF0(_CRT_WARN, fmt)
#define WMSG1(fmt, arg0) _RPTF1(_CRT_WARN, fmt, arg0)
#define WMSG2(fmt, arg0, arg1) _RPTF2(_CRT_WARN, fmt, arg0, arg1)
#define WMSG3(fmt, arg0, arg1, arg2) _RPTF3(_CRT_WARN, fmt, arg0, arg1, arg2)
#define WMSG4(fmt, arg0, arg1, arg2, arg3) _RPTF4(_CRT_WARN, fmt, arg0, arg1, \
             arg2, arg3)
#if !defined(_RPTF5)
# if defined(_DEBUG)
#  define _RPTF5(rptno, msg, arg1, arg2, arg3, arg4, arg5) \
        do { if ((1 == _CrtDbgReport(rptno, __FILE__, __LINE__, NULL, msg, \
                                        arg1, arg2, arg3, arg4, arg5))) \
                _CrtDbgBreak(); } while (0)
# else
#  define _RPTF5(rptno, msg, arg1, arg2, arg3, arg4, arg5)
# endif
#endif

#define WMSG5(fmt, arg0, arg1, arg2, arg3, arg4) _RPTF5(_CRT_WARN, fmt, arg0, \
            arg1, arg2, arg3, arg4)
#endif

/* make the memory leak test output more interesting */
#ifdef _DEBUG
# define new DEBUG_NEW
#endif

/* retain some level of assertion with the non-debug MFC libs */
#if !defined(_DEBUG) && !defined(NDEBUG)
# undef ASSERT
# define ASSERT assert
#endif

/* put this in to break on interesting events when built debug */
#if defined(_DEBUG)
# define DebugBreak() { assert(false); }
#else
# define DebugBreak() ((void) 0)
#endif

#endif /*UTIL_MYDEBUG_H*/
