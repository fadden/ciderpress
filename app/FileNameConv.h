/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * File name conversion.
 */
#ifndef __FILENAMECONV__
#define __FILENAMECONV__

#include "GenericArchive.h"

#define kUnknownTypeStr "???"

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
        fStoredPathName = ":BOGUS:";
        fStoredFssep = '[';
        fFileType = 256;
        fAuxType = 65536;
        fThreadKind = 0;

        fLocalPathName = ":HOSED:";
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
        fLocalPathName = ":HOSED:";
        fLocalFssep = ']';
        // I expect these to be as-yet unset; check it
        ASSERT(!fPreservation);
        ASSERT(!fAddExtension);
        ASSERT(!fJunkPaths);
    }

    // init the "add to archive" side
    void Init(const char* localPathName) {
        //ASSERT(basePathName[strlen(basePathName)-1] != kLocalFssep);
        //fLocalPathName = localPathName + strlen(basePathName)+1;
        fLocalPathName = localPathName;
        fLocalFssep = kLocalFssep;
        // reset the "output" fields
        fStoredPathName = ":HOSED:";
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
    static const char* FileTypeString(unsigned long fileType);
    static const char* FileTypeDescription(long fileType, long auxType);

private:
    void Win32NormalizeFileName(const char* srcp, long srcLen,
        char fssep, char** pDstp, long dstLen);
    void NormalizeFileName(const char* srcp, long srcLen,
        char fssep, char** pDstp, long dstLen);
    void NormalizeDirectoryName(const char* srcp, long srcLen,
        char fssep, char** pDstp, long dstLen);
    void AddPreservationString(const char* pathBuf, char* extBuf);
    void AddTypeExtension(const char* pathBuf, char* extBuf);

    void ReplaceFssep(char* str, char oldc, char newc, char newSubst);
    void LookupExtension(const char* ext);
    bool ExtractPreservationString(char* pathName);
    void InterpretExtension(const char* pathName);
    void DenormalizePath(char* pathBuf);
    void StripDiskImageSuffix(char* pathName);
};

#endif /*__FILENAMECONV__*/