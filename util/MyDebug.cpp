/*
 * CiderPress
 * Copyright (C) 2014 by CiderPress authors.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Debug log support.
 */
#include "stdafx.h"

DebugLog::DebugLog(const WCHAR* logFile)
    : fLogFp(NULL)
{
    fPid = getpid();

    if (logFile == NULL) {
        return;
    }

    PathName debugPath(logFile);
    time_t now = time(NULL);
    time_t when = debugPath.GetModWhen();
    if (when > 0 && now - when > kStaleLog) {
        /* log file is more than 8 hours old, remove it */
        /* [consider opening it and truncating with chsize() instead, so we
        don't hose somebody's custom access permissions. ++ATM 20041015] */
        _wunlink(logFile);
    }
    fLogFp = _wfopen(logFile, L"a");
    if (fLogFp == NULL) {
        _CrtDbgReport(_CRT_WARN, __FILE__, __LINE__, NULL,
            "Unable to open %ls: %d\n", logFile, errno);
    } else {
        // disable buffering so we don't lose anything if app crashes
        setvbuf(fLogFp, NULL, _IONBF, 0);

        fprintf(fLogFp, "\n");
        if (when > 0) {
            fprintf(fLogFp,
                "(Log file was %.3f hours old; logs are reset after %.3f)\n",
                (now - when) / 3600.0, kStaleLog / 3600.0);
        }
    }
}

DebugLog::~DebugLog() {
    if (fLogFp != NULL) {
        fclose(fLogFp);
    }
}

void DebugLog::Log(LogSeverity severity, const char* file, int line,
    const char* format, ...)
{
    if (fLogFp == NULL) {
        return;
    }

    static const char kSeverityChars[] = "?VDIWE";
    if (severity < 0 || severity > sizeof(kSeverityChars) - 1) {
        severity = LOG_UNKNOWN;
    }

    struct tm tmbuf;
    time_t now = time(NULL);
    localtime_s(&tmbuf, &now);

    va_list argptr;
    va_start(argptr, format);

    // had %05u fPid before; not sure that's useful
    fprintf(fLogFp, "%02d:%02d:%02d %c ", tmbuf.tm_hour,
        tmbuf.tm_min, tmbuf.tm_sec, kSeverityChars[severity]);
    vfprintf(fLogFp, format, argptr);
}
