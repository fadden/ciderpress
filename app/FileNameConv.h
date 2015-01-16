/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * File name conversion.
 * TODO: rename to PathProposal.h
 */
#ifndef APP_FILENAMECONV_H
#define APP_FILENAMECONV_H

#include "GenericArchive.h"


/*
 * Proposal for an output pathname, based on the contents of a GenericEntry.
 */
class PathProposal {
public:
    typedef GenericEntry::RecordKind RecordKind;
    enum {
        kDefaultStoredFssep = ':',
        kLocalFssep = '\\',         // PATH_SEP
        kAltLocalFssep = '/'        // PATH_SEP2
    };

    PathProposal(void) {
        fStoredPathName = L":BOGUS:";
        fStoredFssep = '[';
        fFileType = 256;
        fAuxType = 65536;
        fThreadKind = 0;

        fLocalPathName = L":HOSED:";
        fLocalFssep = ']';

        fPreservation = false;
        fAddExtension = false;
        fJunkPaths = false;
        fStripDiskImageSuffix = false;
    }
    virtual ~PathProposal(void) {}

    // init the "extract from archive" side from a GenericEntry struct
    void Init(GenericEntry* pEntry);

    // init the "add to archive" side
    void Init(const WCHAR* localPathName);

    /*
     * Convert a pathname pulled out of an archive to something suitable for the
     * local filesystem.
     *
     * The new pathname may be shorter (because characters were removed) or
     * longer (if we add a "#XXYYYYZ" extension or replace chars with '%' codes).
     */
    void ArchiveToLocal(void);

    /*
     * Convert a local path into something suitable for storage in an archive.
     * Type preservation strings are interpreted and stripped as appropriate.
     *
     * This does *not* do filesystem-specific normalization here.  (It could, but
     * it's better to leave that for later so we can do uniqueification.)
     *
     * In the current implementation, fStoredPathName will always get smaller,
     * but it would be unwise to rely on that.
     */
    void LocalToArchive(const AddFilesDialog* pAddOpts);

    /*
     * Fields for the "archive" side.
     */
    // pathname from record or full pathname from disk image
    CString         fStoredPathName;
    // filesystem separator char (or '\0' for things like DOS 3.3)
    char            fStoredFssep;
    // file type, aux type, and what piece of the file this is
    uint32_t        fFileType;
    uint32_t        fAuxType;
    int             fThreadKind;    // GenericEntry, e.g. kDataThread

    /*
     * Fields for the "local" Side.
     */
    // relative path of file for local filesystem
    CString         fLocalPathName;
    // filesystem separator char for new path (always '\\' for us)
    char            fLocalFssep;

    /*
     * Flags.
     */
    // filename/filetype preservation flags
    bool            fPreservation;
    bool            fAddExtension;
    bool            fJunkPaths;
    bool            fStripDiskImageSuffix;

    /*
     * Return a pointer to the three-letter representation of the file type name.
     */
    static const WCHAR* FileTypeString(uint32_t fileType);

    /*
     * Find an entry in the type description table that matches both file type and
     * aux type.  If no match is found, NULL is returned.
     */
    static const WCHAR* FileTypeDescription(long fileType, long auxType);

private:
    /*
     * Filename normalization for Win32 filesystems.  You can't use [ \/:*?"<>| ]
     * or control characters, and we're currently avoiding high-ASCII stuff.
     */
    void Win32NormalizeFileName(const WCHAR* srcp, long srcLen,
        char fssep, WCHAR** pDstp, long dstLen);

    /*
     * Normalize a file name to local filesystem conventions.  The input
     * is quite possibly *NOT* null-terminated, since it may represent a
     * substring of a full pathname.  Use "srcLen".
     *
     * The output filename is copied to *pDstp, which is advanced forward.
     *
     * The output buffer must be able to hold 3x the original string length.
     */
    void NormalizeFileName(const WCHAR* srcp, long srcLen,
        char fssep, WCHAR** pDstp, long dstLen);

    /*
     * Normalize a directory name to local filesystem conventions.
     */
    void NormalizeDirectoryName(const WCHAR* srcp, long srcLen,
        char fssep, WCHAR** pDstp, long dstLen);

    /*
     * Add a preservation string.
     *
     * "pathBuf" is assumed to have enough space to hold the current path
     * plus kMaxPathGrowth more.  It will be modified in place.
     */
    void AddPreservationString(const WCHAR* pathBuf, WCHAR* extBuf);

    /*
     * Add a ".type" extension to the filename.
     *
     * We either need to retain the existing extension (possibly obscured by file
     * type preservation) or append an extension based on the ProDOS file type.
     */
    void AddTypeExtension(const WCHAR* pathBuf, WCHAR* extBuf);

    /*
     * Replace "oldc" with "newc".  If we find an instance of "newc" already
     * in the string, replace it with "newSubst".
     */
    void ReplaceFssep(WCHAR* str, char oldc, char newc, char newSubst);

    /*
     * Try to figure out what file type is associated with a filename extension.
     *
     * This checks the standard list of ProDOS types (which should catch things
     * like "TXT" and "BIN") and the separate list of recognized extensions.
     */
    void LookupExtension(const WCHAR* ext);

    /*
     * Try to associate some meaning with the file extension.
     */
    void InterpretExtension(const WCHAR* pathName);

    /*
     * Check to see if there's a preservation string on the filename.  If so,
     * set the filetype and auxtype information, and trim the preservation
     * string off.
     */
    bool ExtractPreservationString(WCHAR* pathName);

    /*
     * Remove NuLib2's normalization magic (e.g. "%2f" for '/').
     *
     * This always results in the filename staying the same length or getting
     * smaller, so we can do it in place in the buffer.
     */
    void DenormalizePath(WCHAR* pathBuf);

    /*
     * Remove a disk image suffix.
     *
     * Useful when adding disk images directly from a .SDK or .2MG file.  We
     * don't want them to retain their original suffix.
     */
    void StripDiskImageSuffix(WCHAR* pathName);
};

#endif /*APP_FILENAMECONV_H*/
