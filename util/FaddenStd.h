/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Standard stuff.
 */
#ifndef UTIL_FADDENSTD_H
#define UTIL_FADDENSTD_H

#define NELEM(x) (sizeof(x) / sizeof(x[0]))

/*
 * Declare copy construction and operator=.  Put this in a private section
 * of a class declaration to prevent objects from being copied.
 */
#define DECLARE_COPY_AND_OPEQ(_TYPE) \
    _TYPE(const _TYPE&); \
    _TYPE& operator= (const _TYPE&);

#endif /*UTIL_FADDENSTD_H*/
