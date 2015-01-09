/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of DiskImgLib globals.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"
#include "ASPI.h"

/*static*/ bool Global::fAppInitCalled = false;

/*static*/ ASPI* Global::fpASPI = NULL;

/* global constant */
const char* DiskImgLib::kASPIDev = "ASPI:";


/*
 * Perform one-time DLL initialization.
 */
/*static*/ DIError Global::AppInit(void)
{
    NuError nerr;
    int32_t major, minor, bug;

    if (fAppInitCalled) {
        LOGW("DiskImg AppInit already called");
        return kDIErrNone;
    }

    LOGI("Initializing DiskImg library v%d.%d.%d",
        kDiskImgVersionMajor, kDiskImgVersionMinor, kDiskImgVersionBug);

#ifdef _WIN32
    HMODULE hModule;
    WCHAR fileNameBuf[256];
    hModule = ::GetModuleHandle(L"DiskImg4.dll");
    if (hModule != NULL &&
        ::GetModuleFileName(hModule, fileNameBuf,
            sizeof(fileNameBuf) / sizeof(WCHAR)) != 0)
    {
        // GetModuleHandle does not increase ref count, so no need to release
        LOGD("DiskImg DLL loaded from '%ls'", fileNameBuf);
    } else {
        LOGW("Unable to get DiskImg DLL filename");
    }
#endif

    /*
     * Make sure we're linked against a compatible version of NufxLib.
     */
    nerr = NuGetVersion(&major, &minor, &bug, NULL, NULL);
    if (nerr != kNuErrNone) {
        LOGE("Unable to get version number from NufxLib.");
        return kDIErrNufxLibInitFailed;
    }

    if (major != kNuVersionMajor || minor < kNuVersionMinor) {
        LOGE("Unexpected NufxLib version %d.%d.%d",
                major, minor, bug);
        return kDIErrNufxLibInitFailed;
    }

    /*
     * Do one-time init over in the DiskImg class.
     */
    DiskImg::CalcNibbleInvTables();

#if defined(HAVE_WINDOWS_CDROM) && defined(WANT_ASPI)
    if (kAlwaysTryASPI || IsWin9x()) {
        fpASPI = new ASPI;
        if (fpASPI->Init() != kDIErrNone) {
            delete fpASPI;
            fpASPI = NULL;
        }
    }
#endif
    LOGD("DiskImg HasSPTI=%d HasASPI=%d", GetHasSPTI(), GetHasASPI());

    fAppInitCalled = true;

    return kDIErrNone;
}

/*
 * Perform cleanup at application shutdown time.
 */
/*static*/ DIError Global::AppCleanup(void)
{
    LOGI("DiskImgLib cleanup");
    delete fpASPI;
    return kDIErrNone;
}

/*
 * Simple getters.
 *
 * SPTI is enabled if we're in Win2K *and* ASPI isn't loaded.  If ASPI is
 * loaded, it can interfere with SPTI, so we want to stick with one or
 * the other.
 */
#ifdef _WIN32
/*static*/ bool Global::GetHasSPTI(void) { return !IsWin9x() && fpASPI == NULL; }
/*static*/ bool Global::GetHasASPI(void) { return fpASPI != NULL; }
/*static*/ unsigned long Global::GetASPIVersion(void) {
    assert(fpASPI != NULL);
#ifdef WANT_ASPI
    return fpASPI->GetVersion();
#else
    return 123456789;
#endif
}
#else
/*static*/ bool Global::GetHasSPTI(void) { return false; }
/*static*/ bool Global::GetHasASPI(void) { return false; }
/*static*/ unsigned long Global::GetASPIVersion(void) { assert(false); return 0; }
#endif


/*
 * Return current library versions.
 */
/*static*/ void Global::GetVersion(int32_t* pMajor, int32_t* pMinor,
    int32_t* pBug)
{
    if (pMajor != NULL)
        *pMajor = kDiskImgVersionMajor;
    if (pMinor != NULL)
        *pMinor = kDiskImgVersionMinor;
    if (pBug != NULL)
        *pBug = kDiskImgVersionBug;
}


/*
 * Pointer to debug message handler function.
 */
/*static*/ Global::DebugMsgHandler Global::gDebugMsgHandler = NULL;

/*
 * Change the debug message handler.  The previous handler is returned.
 */
Global::DebugMsgHandler Global::SetDebugMsgHandler(DebugMsgHandler handler)
{
    DebugMsgHandler oldHandler;

    oldHandler = gDebugMsgHandler;
    gDebugMsgHandler = handler;
    return oldHandler;
}

/*
 * Send a debug message to the debug message handler.
 *
 * Even if _DEBUG_MSGS is disabled we can still get here from the NuFX error
 * handler.
 */
/*static*/ void Global::PrintDebugMsg(const char* file, int line, const char* fmt, ...)
{
    if (gDebugMsgHandler == NULL) {
        /*
         * This can happen if the app decides to bail with an exit()
         * call.  I'm not sure what's zapping the pointer.
         *
         * We get here on "-install" or "-uninstall", which really
         * should be using a more Windows-friendly exit strategy.
         */
        DebugBreak();
        return;
    }

    char buf[512];
    va_list args;

    va_start(args, fmt);
#if defined(HAVE_VSNPRINTF)
    (void) vsnprintf(buf, sizeof(buf), fmt, args);
#elif defined(HAVE__VSNPRINTF)
    (void) _vsnprintf(buf, sizeof(buf), fmt, args);
#else
# error "hosed"
#endif
    va_end(args);

    buf[sizeof(buf)-1] = '\0';

    (*gDebugMsgHandler)(file, line, buf);
}
