/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Miscellaneous NufxLib utility functions.
 */
#define __MiscUtils_c__
#include "NufxLibPriv.h"

/*
 * Big fat hairy global.  Unfortunately this is unavoidable.
 */
NuCallback gNuGlobalErrorMessageHandler = nil;


static const char* kNufxLibName = "nufxlib";


/*
 * strerror() equivalent for NufxLib errors.
 */
const char*
Nu_StrError(NuError err)
{
    /*
     * BUG: this should be set up as per-thread storage in an MT environment.
     * I would be more inclined to worry about this if I was expecting
     * it to be used.  So long as valid values are passed in, and the
     * switch statement is kept up to date, we should never have cause
     * to return this.
     *
     * An easier solution, should this present a problem for someone, would
     * be to have the function return nil or "unknown error" when the
     * error value isn't recognized.  I'd recommend leaving it as-is for
     * debug builds, though, as it's helpful to know *which* error is not
     * recognized.
     */
    static char defaultMsg[32];

    switch (err) {
    case kNuErrNone:
        return "(no error)";

    case kNuErrGeneric:
        return "NufxLib generic error";
    case kNuErrInternal:
        return "NufxLib internal error";
    case kNuErrUsage:
        return "NufxLib usage error";
    case kNuErrSyntax:
        return "NufxLib syntax error";
    case kNuErrMalloc:
        return "NufxLib malloc error";
    case kNuErrInvalidArg:
        return "Invalid arguments to NufxLib";
    case kNuErrBadStruct:
        return "Bad NuArchive structure passed to NufxLib";
    case kNuErrBusy:
        return "Attempted invalid reentrant call";

    case kNuErrSkipped:
        return "Skipped by user";
    case kNuErrAborted:
        return "Processing aborted";
    case kNuErrRename:
        return "User wants to rename file";

    case kNuErrFile:
        return "NufxLib trouble with a file";
    case kNuErrFileOpen:
        return "NufxLib unable to open file";
    case kNuErrFileClose:
        return "NufxLib unable to close file";
    case kNuErrFileRead:
        return "NufxLib unable to read file";
    case kNuErrFileWrite:
        return "NufxLib unable to write file";
    case kNuErrFileSeek:
        return "NufxLib unable to seek file";
    case kNuErrFileExists:
        return "File already exists";
    case kNuErrFileNotFound:
        return "No such file or directory";
    case kNuErrFileStat:
        return "Couldn't get file info";
    case kNuErrFileNotReadable:
        return "Read access denied";

    case kNuErrDirExists:
        return "Directory already exists";
    case kNuErrNotDir:
        return "Not a directory";
    case kNuErrNotRegularFile:
        return "Not a regular file";
    case kNuErrDirCreate:
        return "Unable to create directory";
    case kNuErrOpenDir:
        return "Unable to open directory";
    case kNuErrReadDir:
        return "Unable to read directory";
    case kNuErrFileSetDate:
        return "Unable to set file date";
    case kNuErrFileSetAccess:
        return "Unable to set file access";
    case kNuErrFileAccessDenied:
        return "Access denied";

    case kNuErrNotNuFX:
        return "Input is not a NuFX archive";
    case kNuErrBadMHVersion:
        return "Unrecognized Master Header version";
    case kNuErrRecHdrNotFound:
        return "Next record not found";
    case kNuErrNoRecords:
        return "No records in archive";
    case kNuErrBadRecord:
        return "Bad data in record";
    case kNuErrBadMHCRC:
        return "Bad Master Header CRC";
    case kNuErrBadRHCRC:
        return "Bad Record header CRC";
    case kNuErrBadThreadCRC:
        return "Bad Thread header CRC";
    case kNuErrBadDataCRC:
        return "Data CRC mismatch";

    case kNuErrBadFormat:
        return "Thread compression format unsupported";
    case kNuErrBadData:
        return "Bad data found";
    case kNuErrBufferOverrun:
        return "Buffer overrun";
    case kNuErrBufferUnderrun:
        return "Buffer underrun";
    case kNuErrOutMax:
        return "Output limit exceeded";

    case kNuErrNotFound:
        return "Not found";
    case kNuErrRecordNotFound:
        return "Record not found";
    case kNuErrRecIdxNotFound:
        return "RecordIdx not found";
    case kNuErrThreadIdxNotFound:
        return "ThreadIdx not found";
    case kNuErrThreadIDNotFound:
        return "ThreadID not found";
    case kNuErrRecNameNotFound:
        return "Record name not found";
    case kNuErrRecordExists:
        return "Record already exists";

    case kNuErrAllDeleted:
        return "Tried to delete all files";
    case kNuErrArchiveRO:
        return "Archive is in read-only mode";
    case kNuErrModRecChange:
        return "Attempt to alter a modified record";
    case kNuErrModThreadChange:
        return "Attempt to alter a modified thread";
    case kNuErrThreadAdd:
        return "Can't add conflicting threadID";
    case kNuErrNotPreSized:
        return "Operation only permitted on pre-sized threads";
    case kNuErrPreSizeOverflow:
        return "Data exceeds pre-sized thread size";
    case kNuErrInvalidFilename:
        return "Invalid filename";

    case kNuErrLeadingFssep:
        return "Storage name started with fssep char";
    case kNuErrNotNewer:
        return "New item wasn't newer than existing";
    case kNuErrDuplicateNotFound:
        return "Can only update an existing item";
    case kNuErrDamaged:
        return "Original archive may have been damaged";

    case kNuErrIsBinary2:
        return "This is a Binary II archive";

    case kNuErrUnknownFeature:
        return "Unknown feature";
    case kNuErrUnsupFeature:
        return "Feature not supported";

    default:
        sprintf(defaultMsg, "(error=%d)", err);
        return defaultMsg;
    }
}


#define kNuHeftyBufSize 256     /* all error messages should fit in this */
#define kNuExtraGoodies 8       /* leave room for "\0" and other trivial chars*/

/*
 * Similar to perror(), but takes the error as an argument, and knows
 * about NufxLib errors as well as system errors.
 *
 * Depending on the compiler, "file", "line", and "function" may be nil/zero.
 *
 * Calling here with "pArchive"==nil is allowed, but should only be done
 * if the archive is inaccessible (perhaps because it failed to open).  We
 * can't invoke the error message callback if the pointer is nil.
 */
void
Nu_ReportError(NuArchive* pArchive, const char* file, int line,
    const char* function, Boolean isDebug, NuError err, const char* format, ...)
{
    NuErrorMessage errorMessage;
    const char* msg;
    va_list args;
    char buf[kNuHeftyBufSize];
    int count;
    #if !defined(HAVE_SNPRINTF) && defined(SPRINTF_RETURNS_INT)
    int cc;
    #endif

    Assert(format != nil);


    va_start(args, format);

    #if defined(HAVE_VSNPRINTF) && defined(VSNPRINTF_DECLARED)
    count = vsnprintf(buf, sizeof(buf)-kNuExtraGoodies, format, args);
    #else
      #ifdef SPRINTF_RETURNS_INT
        count = vsprintf(buf, format, args);
      #else
        vsprintf(buf, format, args);
        count = strlen(buf);
      #endif
    #endif

    va_end(args);

    Assert(count > 0);
    if (count < 0)
        goto bail;

    /* print the error code data, if any */
    if (err != kNuErrNone) {
        /* we know we have room for ": ", because of kNuExtraGoodies */
        strcpy(buf+count, ": ");
        count += 2;

        msg = nil;
        if (err >= 0)
            msg = strerror(err);
        if (msg == nil)
            msg = Nu_StrError(err);

        #if defined(HAVE_SNPRINTF) && defined(SNPRINTF_DECLARED)
        if (msg == nil)
            snprintf(buf+count, sizeof(buf) - count,
                        "(unknown err=%d)", err);
        else
            snprintf(buf+count, sizeof(buf) - count, "%s", msg);
        #else
          #ifdef SPRINTF_RETURNS_INT
            if (msg == nil)
                cc = sprintf(buf+count, "(unknown err=%d)", err);
            else
                cc = sprintf(buf+count, "%s", msg);
            Assert(cc > 0);
            count += cc;
          #else
            if (msg == nil)
                sprintf(buf+count, "(unknown err=%d)", err);
            else
                sprintf(buf+count, "%s", msg);
            count += strlen(buf + count);
          #endif
        #endif

    }

    #if !defined(HAVE_SNPRINTF) || !defined(HAVE_VSNPRINTF) || \
        !defined(SNPRINTF_DELCARED) || !defined(VSNPRINTF_DECLARED)
    /* couldn't do it right, so check for overflow */
    Assert(count <= kNuHeftyBufSize);
    #endif

    if ((pArchive != nil && pArchive->messageHandlerFunc == nil) ||
        (pArchive == nil && gNuGlobalErrorMessageHandler == nil))
    {
        if (isDebug) {
            fprintf(stderr, "%s: [%s:%d %s] %s\n", kNufxLibName,
                file, line, function, buf);
        } else {
            fprintf(stderr, "%s: ERROR: %s\n", kNufxLibName, buf);
        }
    } else {
        errorMessage.message = buf;
        errorMessage.err = err;
        errorMessage.isDebug = isDebug;
        errorMessage.file = file;
        errorMessage.line = line;
        errorMessage.function = function;
        
        if (pArchive == nil)
            (void) (*gNuGlobalErrorMessageHandler)(pArchive, &errorMessage);
        else
            (void) (*pArchive->messageHandlerFunc)(pArchive, &errorMessage);
    }

bail:
    return;
}


/*
 * Memory allocation wrappers.
 *
 * Under gcc these would be macros, but not all compilers can handle that.
 *
 * [ It should be possible to use mmalloc instead of malloc.  Just tuck the
 *   mmalloc descriptor into the NuArchive struct. ]
 */

#ifndef USE_DMALLOC
void*
Nu_Malloc(NuArchive* pArchive, size_t size)
{
    void* _result;

    Assert(size > 0);
    _result = malloc(size);
    if (_result == nil) {
        Nu_ReportError(NU_BLOB, kNuErrMalloc, "malloc(%u) failed", (uint) size);
        DebugAbort();   /* leave a core dump if we're built for it */
    }
    DebugFill(_result, size);
    return _result;
}

void*
Nu_Calloc(NuArchive* pArchive, size_t size)
{
    void* _cresult = Nu_Malloc(pArchive, size);
    memset(_cresult, 0, size);
    return _cresult;
}

void*
Nu_Realloc(NuArchive* pArchive, void* ptr, size_t size)
{
    void* _result;

    Assert(ptr != nil);     /* disallow this usage */
    Assert(size > 0);       /* disallow this usage */
    _result = realloc(ptr, size);
    if (_result == nil) {
        Nu_ReportError(NU_BLOB, kNuErrMalloc, "realloc(%u) failed",(uint) size);
        DebugAbort();   /* leave a core dump if we're built for it */
    }
    return _result;
}

void
Nu_Free(NuArchive* pArchive, void* ptr)
{
    if (ptr != nil)
        free(ptr);
}
#endif

/*
 * If somebody internal wants to set doClose on a buffer DataSource
 * (looks like "Rename" does), we need to supply a "free" callback.
 */
NuResult
Nu_InternalFreeCallback(NuArchive* pArchive, void* args)
{
    DBUG(("+++ internal free callback 0x%08lx\n", (long) args));
    Nu_Free(nil, args);
    return kNuOK;
}

