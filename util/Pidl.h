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
    /*
     * Concatenates two PIDLs.  The PIDL returned is newly-allocated storage.
     *
     * "pidl1" may be NULL.
     */
    static LPITEMIDLIST ConcatPidls(LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2);

    /*
     * Get a fully qualified PIDL for a ShellFolder.
     *
     * This is a rather roundabout way of doing things (converting to a full
     * display name and then converting that to a PIDL).  However, there doesn't
     * seem to be a way to just ask a ShellFolder for its fully qualified PIDL.
     * TODO: see if there's a better way now.
     *
     * Pass in the parent ShellFolder and the item's partial PIDL.
     */
    static LPITEMIDLIST GetFullyQualPidl(LPSHELLFOLDER lpsf, LPITEMIDLIST lpi);

    /*
     * Make a copy of a PIDL.
     *
     * The PIDL returned is newly-allocated storage.
     */
    static LPITEMIDLIST CopyITEMID(LPMALLOC lpMalloc, LPITEMIDLIST lpi);

    /*
     * Get the display name of a file in a ShellFolder.
     *
     * "lpsf" is the ShellFolder that contains the file, "lpi" is the PIDL for
     * the file, "dwFlags" is passed to GetDisplayNameOf and affects which
     * name is returned, and "lpFriendlyName" is a buffer of at least MAX_PATH
     * bytes.
     */
    static BOOL GetName(LPSHELLFOLDER lpsf, LPITEMIDLIST lpi, DWORD dwFlags,
        CString* pFriendlyName);

    /*
     * Allocate a PIDL of the specified size.
     */
    static LPITEMIDLIST CreatePidl(UINT cbSize);

    /*
     * Compute the size in bytes of a PIDL.
     */
    static UINT GetSize(LPCITEMIDLIST pidl);

    /*
     * Return the next item in the PIDL.
     *
     * "pidl->mkid.cb" will be zero at the end of the list.
     */
    static LPITEMIDLIST Next(LPCITEMIDLIST pidl);

    /*
     * Convert a PIDL to a filesystem path.
     *
     * Returns TRUE on success, FALSE on failure.
     */
    static BOOL GetPath(LPCITEMIDLIST pidl, CString* pPath);

    /*
     * Get the index for an icon for a ShellFolder object.
     *
     * "lpi" is the fully-qualified PIDL for the object in question.  "uFlags"
     * specifies which of the object's icons to retrieve.
     */
    static int GetItemIcon(LPITEMIDLIST lpi, UINT uFlags);

private:
    Pidl();     // do not instantiate
    ~Pidl();
};

#endif /*UTIL_PIDL_H*/
