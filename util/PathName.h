/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Filename manipulations.  Includes some basic file ops (e.g. tests for
 * file existence) as well.
 */
#ifndef UTIL_PATHNAME_H
#define UTIL_PATHNAME_H

/*
 * Holds a full or partial pathname, manipulating it in various ways.
 *
 * This creates some hefty buffers for _splitpath() to use, so don't use
 * these as storage within other objects.
 *
 * Functions that return CStrings should have their values assigned into
 * CString variables.  Attempting to set a "const char*" to them can cause
 * problems as the CString being returned is in local storage.
 */
class PathName {
public:
    PathName(const WCHAR* pathName = L"", WCHAR fssep = '\\') {
        ASSERT(fssep == '\\');  // not handling other cases yet
        fPathName = pathName;
        fFssep = fssep;
        fSplit = false;
    }
    PathName(const CString& pathName, WCHAR fssep = '\\') {
        ASSERT(fssep == '\\');  // not handling other cases yet
        fPathName = pathName;
        fFssep = fssep;
        fSplit = false;
    }
    ~PathName(void) {}

    /*
     * Name manipulations.
     */
    void SetPathName(const WCHAR* pathName, WCHAR fssep = '\\') {
        ASSERT(fssep == '\\');  // not handling other cases yet
        fPathName = pathName;
        fFssep = fssep;
        fSplit = false;
    }
    void SetPathName(const CString& pathName, WCHAR fssep = '\\') {
        ASSERT(fssep == '\\');  // not handling other cases yet
        fPathName = pathName;
        fFssep = fssep;
        fSplit = false;
    }

    // get the full pathname we have stored
    CString GetPathName(void) const { return fPathName; }

    // create a pathname from a "foreign" OS name
    int ConvertFrom(const char* foreignName, char foreignFssep);

    // return just the filename: "C:\foo\bar.txt" -> "bar.txt"
    CString GetFileName(void);

    // return just the extension: C:\foo\bar.txt --> ".txt"
    CString GetExtension(void);

    // return just the drive component: "C:\foo\bar.txt" --> "C:"
    CString GetDriveOnly(void);

    // return drive and path component: "C:\foo\bar.txt" -> "C:\foo\"
    // (assumes trailing paths end in '\')
    CString GetDriveAndPath(void);
    CString GetPathOnly(void);

    /*
     * Expand the short file name of an existing file into its long form.
     *
     * Returns 0 on success, -1 on failure.
     */
    int SFNToLFN(void);

    // returns the description of the file type (as seen in explorer)
    CString GetDescription(void);

    // determine whether or not the file exists
    bool Exists(void);

    // check the status of a file
    int CheckFileStatus(struct _stat* psb, bool* pExists, bool* pIsReadable,
        bool* pIsDir);

    // get the modification date
    time_t GetModWhen(void);
    // set the modification date
    int SetModWhen(time_t when);

    /*
     * Create subdirectories, if needed.  The paths leading up to the filename
     * in "pathname" will be created.
     *
     * If "pathname" is just a filename, or the set of directories matches
     * the last directory we created, we don't do anything.
     *
     * Returns 0 on success, or a Windows error code on failure.
     */
    int CreatePathIFN(void);

    /*
     * Return the filename extension found in a full pathname.
     *
     * An extension is the stuff following the last '.' in the filename.  If
     * there is nothing following the last '.', then there is no extension.
     *
     * Returns a pointer to the '.' preceding the extension, or NULL if no
     * extension was found.
     *
     * We guarantee that there is at least one character after the '.'.
     */
    static const WCHAR* FindExtension(const WCHAR* pathname, WCHAR fssep);

    /*
     * Find the filename component of a local pathname.  Uses the fssep passed
     * in.  If the fssep is '\0' (as is the case for DOS 3.3), then the entire
     * pathname is returned.
     *
     * Always returns a pointer to a string; never returns NULL.
     */
    static const WCHAR* FilenameOnly(const WCHAR* pathname, WCHAR fssep);

    /*
     * Test to see if a wide-to-narrow filename conversion failed.
     *
     * Returns true if all is well, false with *pErrMsg set if something
     * went wrong.
     */
    static bool TestNarrowConversion(const CString& original,
            const CStringA& converted, CString* pErrMsg) {
        int index = converted.ReverseFind('?');
        if (index < 0) {
            // no '?' is good
        } else if (index == 2 && converted.Left(4) == "\\\\?\\") {
            // Win32 file namespace path strings start with "\\?\".  If that's
            // the first occurrence of '?', we're still good.
        } else {
            // This is most likely the result of a failed wide-to-narrow
            // string conversion.
            pErrMsg->Format(L"Unable to open '%ls' -- Unicode filename "
                            L"conversion is invalid ('%hs')",
                    (LPCWSTR) original, (LPCSTR) converted);
            return false;
        }
        return true;
    }
private:
    DECLARE_COPY_AND_OPEQ(PathName)

    void SplitIFN(void) {
        if (!fSplit) {
            _wsplitpath(fPathName, fDrive, fDir, fFileName, fExt);
            fSplit = true;
        }
    }

    /*
     * Invoke the system-dependent directory creation function.
     */
    int Mkdir(const WCHAR* dir);

    /*
     * Determine if a file exists, and if so whether or not it's a directory.
     *
     * Set fields you're not interested in to NULL.
     *
     * On success, returns 0 and fields are set appropriately.  On failure,
     * returns nonzero and result values are undefined.
     */
    int GetFileInfo(const WCHAR* pathname, struct _stat* psb, time_t* pModWhen,
        bool* pExists, bool* pIsReadable, bool* pIsDirectory);

    /*
     * Create a single subdirectory if it doesn't exist.  If the next-highest
     * subdirectory level doesn't exist either, cut down the pathname and
     * recurse.
     *
     * "pathEnd" points at the last valid character.  The length of the valid
     * path component is therefore (pathEnd-pathStart+1).
     */
    int CreateSubdirIFN(const WCHAR* pathStart, const WCHAR* pathEnd,
        WCHAR fssep);

    CString     fPathName;
    WCHAR       fFssep;
    bool        fSplit;

    WCHAR       fDrive[_MAX_DRIVE];         // 3
    WCHAR       fDir[_MAX_DIR];             // 256
    WCHAR       fFileName[_MAX_FNAME];      // 256
    WCHAR       fExt[_MAX_EXT];             // 256
};

#endif /*UTIL_PATHNAME_H*/
