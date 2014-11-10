/*
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Misc stuff (shared between nufxlib and nulib2).  This is a collection
 * of standard functions that aren't available in libc on this system.
 */
#include "SysDefs.h"
#include "MiscStuff.h"
#include <ctype.h>


#ifndef HAVE_STRERROR
/*
 * Return a pointer to the appropriate string in the system table, or NULL
 * if the value is out of bounds.
 */
const char*
Nu_strerror(int errnum)
{
    extern int sys_nerr;
    extern char *sys_errlist[];

    if (errnum < 0 || errnum > sys_nerr)
        return NULL;

    return sys_errlist[errnum];
}
#endif

#ifndef HAVE_MEMMOVE
/*
 * Move a block of memory.  Unlike memcpy, this is expected to work
 * correctly with overlapping blocks.
 *
 * This is a straightforward implementation.  A much faster implementation,
 * from BSD, is available in the PGP 2.6.2 distribution, but this should
 * suffice for those few systems that don't have memmove.
 */
void*
Nu_memmove(void* dst, const void* src, size_t n)
{
    void* retval = dst;
    char* srcp = (char*)src;
    char* dstp = (char*)dst;

    /* you can normally get away with this if n==0 */
    Assert(dst != NULL);
    Assert(src != NULL);

    if (dstp == srcp || !n) {
        /* nothing to do */
    } else if (dstp > srcp) {
        /* start from the end */
        (char*)dstp += n-1;
        (char*)srcp += n-1;
        while (n--)
            *dstp-- = *srcp--;
    } else {
        /* start from the front */
        while (n--)
            *dstp++ = *srcp++;
    }

    return retval;
}
#endif

#ifndef HAVE_STRTOUL
/*
 * Perform strtol, but on an unsigned long.
 *
 * On systems that have strtol but don't have strtoul, the strtol
 * function doesn't clamp the return value, making it similar in
 * function to strtoul.  The comparison is not exact, however,
 * because strtoul is expected to lots of fancy things (like set
 * errno to ERANGE).
 *
 * For our purposes here, strtol does all we need it to.  Someday
 * we should replace this with a "real" version.
 */
unsigned long
Nu_strtoul(const char *nptr, char **endptr, int base)
{
    return strtol(nptr, endptr, base);
}
#endif

#ifndef HAVE_STRCASECMP
/*
 * Compare two strings, case-insensitive.
 */
int
Nu_strcasecmp(const char *str1, const char *str2)
{
    while (*str1 && *str2 && toupper(*str1) == toupper(*str2))
        str1++, str2++;
    return (toupper(*str1) - toupper(*str2));
}

#endif

#ifndef HAVE_STRNCASECMP
/*
 * Compare two strings, case-insensitive, stopping after "n" chars.
 */
int
Nu_strncasecmp(const char *str1, const char *str2, size_t n)
{
    while (n && *str1 && *str2 && toupper(*str1) == toupper(*str2))
        str1++, str2++, n--;

    if (n)
        return (toupper(*str1) - toupper(*str2));
    else
        return 0;   /* no mismatch in first n chars */
}
#endif

