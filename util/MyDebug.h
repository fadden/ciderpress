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

#include "PathName.h"
#include "FaddenStd.h"
#include <sal.h>

/*
 * Debug log output.
 *
 * Generally an application will only want one instance of this, which will be
 * accessed through a global variable by log macros.
 */
class DebugLog {
public:
    /*
     * Pass in the log file name, or NULL if logging to a file is not
     * desired.  If the log file cannot be opened, no error is reported,
     * but the Log call will do nothing.
     */
    DebugLog(const WCHAR* logFile);

    ~DebugLog();

    typedef enum {
        LOG_UNKNOWN=0, LOG_VERBOSE=1, LOG_DEBUG=2,
        LOG_INFO=3, LOG_WARN=4, LOG_ERROR=5
    } LogSeverity;

    /*
     * Write a message to the log file.
     *
     * The format string should not end with a newline.  Each log message
     * will appear on its own line, possibly with a prefix.
     */
    void Log(LogSeverity severity, const char* file, int line,
        _Printf_format_string_ const char* format, ...);

private:
    DECLARE_COPY_AND_OPEQ(DebugLog)

    const int kStaleLog = 8 * 60 * 60;      // 8 hours

    FILE*   fLogFp;
    int     fPid;
};

extern DebugLog* gDebugLog;     // declare and allocate in app


/* send the message to the log file (if open) and the CRT debug mechanism */
#define LOG_BASE(severity, file, line, format, ...) \
    { \
        if (gDebugLog != NULL) { \
            gDebugLog->Log((severity), (file), (line), (format), __VA_ARGS__); \
        } \
    }

/*
 * Log macros, with priority specifier.  The output will be written to the
 * log file, if one is open, and to the debugger output window, if available.
 *
 * The verbose-level debugging should be enabled on a file-by-file basis
 * with a compile-time define, but that doesn't seem to work (pre-compiled
 * header interference, maybe?).
 */
//#ifdef SHOW_LOGV
# define LOGV(format, ...) \
    LOG_BASE(DebugLog::LOG_VERBOSE, __FILE__, __LINE__, format, __VA_ARGS__)
//#else
//# define LOGV(format, ...) ((void)0)
//#endif

#define LOGD(format, ...) \
    LOG_BASE(DebugLog::LOG_DEBUG, __FILE__, __LINE__, format, __VA_ARGS__)
#define LOGI(format, ...) \
    LOG_BASE(DebugLog::LOG_INFO, __FILE__, __LINE__, format, __VA_ARGS__)
#define LOGW(format, ...) \
    LOG_BASE(DebugLog::LOG_WARN, __FILE__, __LINE__, format, __VA_ARGS__)
#define LOGE(format, ...) \
    LOG_BASE(DebugLog::LOG_ERROR, __FILE__, __LINE__, format, __VA_ARGS__)

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
# define DebugBreak() { _CrtDbgBreak(); }
#else
# define DebugBreak() ((void) 0)
#endif

#endif /*UTIL_MYDEBUG_H*/
