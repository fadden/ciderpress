/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 */
#include "NufxLibPriv.h"

/* executable was build on or after this date */
#ifdef __DATE__
static const char gNuBuildDate[] = __DATE__;
#else
static const char gNuBuildDate[] = "??? ?? ????";
#endif

#ifdef OPTFLAGSTR
static const char gNuBuildFlags[] = OPTFLAGSTR;
#else
static const char gNuBuildFlags[] = "-";
#endif


/*
 * Return the version number, date built, and build flags.
 */
NuError
Nu_GetVersion(long* pMajorVersion, long* pMinorVersion, long* pBugVersion,
    const char** ppBuildDate, const char** ppBuildFlags)
{
    if (pMajorVersion != nil)
        *pMajorVersion = kNuVersionMajor;
    if (pMinorVersion != nil)
        *pMinorVersion = kNuVersionMinor;
    if (pBugVersion != nil)
        *pBugVersion = kNuVersionBug;
    if (ppBuildDate != nil)
        *ppBuildDate = gNuBuildDate;
    if (ppBuildFlags != nil)
        *ppBuildFlags = gNuBuildFlags;
    return kNuErrNone;
}

