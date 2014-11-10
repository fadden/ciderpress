/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * File name conversion.
 */
#ifndef APP_FILENAMECONV_H
#define APP_FILENAMECONV_H

#include "GenericArchive.h"

#define kUnknownTypeStr L"???"

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
    void Init(GenericEntry* pEntry) {
        fStoredPathName = pEntry->GetPathName();
        fStoredFssep = pEntry->GetFssep();
        //if (fStoredFssep == '\0')         // e.g. embedded DOS 3.3 volume
        //  fStoredFssep = kDefaultStoredFssep;
        fFileType = pEntry->GetFileType();
        fAuxType = pEntry->GetAuxType();
        //fThreadKind set from SelectionEntry
        // reset the "output" fields
        fLocalPathName = L":HOSED:";
        fLocalFssep = ']';
        // I expect these to be as-yet unset; check it
        ASSERT(!fPreservation);
        ASSERT(!fAddExtension);
        ASSERT(!fJunkPaths);
    }

    // init the "add to archive" side
    void Init(const WCHAR* localPathName) {
        //ASSERT(basePathName[strlen(basePathName)-1] != kLocalFssep);
        //fLocalPathName = localPathName + strlen(basePathName)+1;
        fLocalPathName = localPathName;
        fLocalFssep = kLocalFssep;
        // reset the "output" fields
        fStoredPathName = L":HOSED:";
        fStoredFssep = '[';
        fFileType = 0;
        fAuxType = 0;
        fThreadKind = GenericEntry::kDataThread;
        // I expect these to be as-yet unset; check it
        ASSERT(!fPreservation);
        ASSERT(!fAddExtension);
        ASSERT(!fJunkPaths);
    }

    // Convert a partial pathname from the archive to a local partial path.
    void ArchiveToLocal(void);
    // Same thing, other direction.
    void LocalToArchive(const AddFilesDialog* pAddOpts);

    /*
     * Fields for the "archive" side.
     */
    // pathname from record or full pathname from disk image
    CString         fStoredPathName;
    // filesystem separator char (or '\0' for things like DOS 3.3)
    char            fStoredFssep;
    // file type, aux type, and what piece of the file this is
    unsigned long   fFileType;
    unsigned long   fAuxType;
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
     * Misc utility functions.
     */
    static const WCHAR* FileTypeString(unsigned long fileType);
    static const WCHAR* FileTypeDescription(long fileType, long auxType);

private:
    void Win32NormalizeFileName(const WCHAR* srcp, long srcLen,
        char fssep, WCHAR** pDstp, long dstLen);
    void NormalizeFileName(const WCHAR* srcp, long srcLen,
        char fssep, WCHAR** pDstp, long dstLen);
    void NormalizeDirectoryName(const WCHAR* srcp, long srcLen,
        char fssep, WCHAR** pDstp, long dstLen);
    void AddPreservationString(const WCHAR* pathBuf, WCHAR* extBuf);
    void AddTypeExtension(const WCHAR* pathBuf, WCHAR* extBuf);

    void ReplaceFssep(WCHAR* str, char oldc, char newc, char newSubst);
    void LookupExtension(const WCHAR* ext);
    bool ExtractPreservationString(WCHAR* pathName);
    void InterpretExtension(const WCHAR* pathName);
    void DenormalizePath(WCHAR* pathBuf);
    void StripDiskImageSuffix(WCHAR* pathName);
};

#endif /*APP_FILENAMECONV_H*/
