/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * A collection of functions for manipulating Pointers to ID Lists (PIDLs).
 */
#ifndef UTIL_PIDL_H
#define UTIL_PIDL_H

/*
 * All functions are static; the class is more about namespace protection than
 * data encapsulation.
 */
class Pidl {
public:
    // Functions that deal with PIDLs
    static LPITEMIDLIST ConcatPidls(LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2);
    static LPITEMIDLIST GetFullyQualPidl(LPSHELLFOLDER lpsf, LPITEMIDLIST lpi);
    static LPITEMIDLIST CopyITEMID(LPMALLOC lpMalloc, LPITEMIDLIST lpi);
    static BOOL GetName(LPSHELLFOLDER lpsf, LPITEMIDLIST lpi, DWORD dwFlags,
        CString* pFriendlyName);
    static LPITEMIDLIST CreatePidl(UINT cbSize);
    static UINT GetSize(LPCITEMIDLIST pidl);
    static LPITEMIDLIST Next(LPCITEMIDLIST pidl);
    static BOOL GetPath(LPCITEMIDLIST pidl, CString* pPath);

    // Utility Functions
    //static BOOL DoTheMenuThing(HWND hwnd, LPSHELLFOLDER lpsfParent,
    //  LPITEMIDLIST lpi, LPPOINT lppt);
    static int GetItemIcon(LPITEMIDLIST lpi, UINT uFlags);
};

#endif /*UTIL_PIDL_H*/
