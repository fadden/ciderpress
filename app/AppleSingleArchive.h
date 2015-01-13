/*
 * CiderPress
 * Copyright (C) 2015 by faddenSoft.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * AppleSingle support.  This format provides a way to package a single
 * forked file into an ordinary file.
 *
 * To create a test file from Mac OS X using NuLib2 v3.0 or later:
 * - extract a forked file with "nulib2 xe <archive.shk> <file>"
 * - rename the type-preservation header off of <file>'s data fork
 * - combine the forks with "cat <file>#nnnnr > <file>/..namedfork/rsrc"
 * - use "xattr -l <file>" to confirm that the file has a resource fork
 *   and the FinderInfo with the ProDOS file type
 * - use "applesingle encode <file>" to create <file>.as
 *
 * The tool does not create a spec-compliant AppleSingle file.  The v2
 * spec is mildly ambiguous, but the Apple II file type note says,
 * "...which is stored reverse as $00 $05 $16 $00".  It appears that
 * someone decided to generate little-endian AppleSingle files, and you
 * have to use the magic number to figure out which end is which.
 * FWIW, the Linux "file" command only recognizes the big-endian form.
 *
 * Perhaps unsurprisingly, the "applesingle" tool is not able to decode the
 * files it creates -- but it can handle files GS/ShrinkIt creates.
 *
 * The GS/ShrinkIt "create AppleSingle" function creates a version 1 file
 * with Mac OS Roman filenames.  The Mac OS X tool creates a version 2 file
 * with UTF-8-encoded Unicode filenames.  We will treat the name
 * accordingly, though it's possible there are v2 files with MOR strings.
 */
#ifndef APP_APPLESINGLEARCHIVE_H
#define APP_APPLESINGLEARCHIVE_H

#include "GenericArchive.h"


class AppleSingleArchive;

/*
 * AppleSingle files only have one entry, so making this a separate class
 * is just in keeping with the overall structure.
 */
class AppleSingleEntry : public GenericEntry {
public:
    AppleSingleEntry(AppleSingleArchive* pArchive) :
        fpArchive(pArchive), fDataOffset(-1), fRsrcOffset(-1) {}
    virtual ~AppleSingleEntry(void) {}

    virtual int ExtractThreadToBuffer(int which, char** ppText, long* pLength,
        CString* pErrMsg) const override;
    virtual int ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
        ConvertHighASCII convHA, CString* pErrMsg) const override;

    // doesn't matter
    virtual long GetSelectionSerial(void) const override { return -1; }

    virtual bool GetFeatureFlag(Feature feature) const override {
        if (feature == kFeatureHasFullAccess ||
            feature == kFeatureHFSTypes)
        {
            return true;
        } else {
            return false;
        }
    }

    void SetDataOffset(long offset) { fDataOffset = offset; }
    void SetRsrcOffset(long offset) { fRsrcOffset = offset; }

private:
    /*
     * Copy data from the seeked archive to outfp, possibly converting EOL along
     * the way.
     */
    int CopyData(long srcLen, FILE* outfp, ConvertEOL conv,
        ConvertHighASCII convHA, CString* pMsg) const;

    AppleSingleArchive* fpArchive;      // holds FILE* for archive
    long                fDataOffset;
    long                fRsrcOffset;
};


/*
 * AppleSingle archive definition.
 */
class AppleSingleArchive : public GenericArchive {
public:
    AppleSingleArchive(void) : fFp(NULL), fEntries(NULL), fIsBigEndian(false) {}
    virtual ~AppleSingleArchive(void) {
        (void) Close();
        delete[] fEntries;
    }

    /*
     * Perform one-time initialization.  There really isn't any for us.
     *
     * Returns an error string on failure.
     */
    static CString AppInit(void);

    /*
     * Open an AppleSingle archive.
     *
     * Returns an error string on failure, or "" on success.
     */
    virtual OpenResult Open(const WCHAR* filename, bool readOnly,
        CString* pErrMsg) override;

    /*
     * Create a new AppleSingleArchive instance.
     *
     * This isn't implemented, and will always return an error.
     */
    virtual CString New(const WCHAR* filename, const void* options) override;

    virtual CString Flush(void) override { return L""; }

    virtual CString Reload(void) override;
    virtual bool IsReadOnly(void) const override { return true; };
    virtual bool IsModified(void) const override { return false; }
    virtual CString GetDescription() const override { return L"AppleSingle"; }
    virtual bool BulkAdd(ActionProgressDialog* pActionProgress,
        const AddFilesDialog* pAddOpts) override
        { ASSERT(false); return false; }
    virtual bool AddDisk(ActionProgressDialog* pActionProgress,
        const AddFilesDialog* pAddOpts) override
        { ASSERT(false); return false; }
    virtual bool CreateSubdir(CWnd* pMsgWnd, GenericEntry* pParentEntry,
        const WCHAR* newName) override
        { ASSERT(false); return false; }
    virtual bool TestSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) override
        { ASSERT(false); return false; }
    virtual bool DeleteSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) override
        { ASSERT(false); return false; }
    virtual bool RenameSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) override
        { ASSERT(false); return false; }
    virtual bool RenameVolume(CWnd* pMsgWnd, DiskFS* pDiskFS,
        const WCHAR* newName) override
        { ASSERT(false); return false; }
    virtual CString TestVolumeName(const DiskFS* pDiskFS,
        const WCHAR* newName) const override
        { ASSERT(false); return L"!"; }
    virtual CString TestPathName(const GenericEntry* pGenericEntry,
        const CString& basePath, const CString& newName, char newFssep) const override
        { ASSERT(false); return L"!"; }
    virtual bool RecompressSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
        const RecompressOptionsDialog* pRecompOpts) override
        { ASSERT(false); return false; }
    virtual XferStatus XferSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
        ActionProgressDialog* pActionProgress,
        const XferFileOptions* pXferOpts) override
        { ASSERT(false); return kXferFailed; }
    virtual bool GetComment(CWnd* pMsgWnd, const GenericEntry* pEntry,
        CString* pStr) override
        { ASSERT(false); return false; }
    virtual bool SetComment(CWnd* pMsgWnd, GenericEntry* pEntry,
        const CString& str) override
        { ASSERT(false); return false; }
    virtual bool DeleteComment(CWnd* pMsgWnd, GenericEntry* pEntry) override
        { ASSERT(false); return false; }
    virtual bool SetProps(CWnd* pMsgWnd, GenericEntry* pEntry,
        const FileProps* pProps) override
        { ASSERT(false); return false; }
    virtual void PreferencesChanged(void) override {}
    virtual long GetCapability(Capability cap) override;

    // Generate a string for the "archive info" dialog.
    CString GetInfoString();

    friend class AppleSingleEntry;

private:
    // File header.  "homeFileSystem" became all-zero "filler" in v2.
    static const int kHomeFileSystemLen = 16;
    static const int kMagicNumber = 0x00051600;
    static const int kVersion1 = 0x00010000;
    static const int kVersion2 = 0x00020000;
    struct FileHeader {
        uint32_t    magic;
        uint32_t    version;
        char        homeFileSystem[kHomeFileSystemLen + 1];
        uint16_t    numEntries;
    };
    static const size_t kHeaderLen = 4 + 4 + kHomeFileSystemLen + 2;

    // Array of these, just past the file header.
    struct TOCEntry {
        uint32_t    entryId;
        uint32_t    offset;
        uint32_t    length;
    };
    static const size_t kEntryLen = 4 + 4 + 4;

    // predefined values for entryId
    enum {
        kIdDataFork             = 1,
        kIdResourceFork         = 2,
        kIdRealName             = 3,
        kIdComment              = 4,
        kIdBWIcon               = 5,
        kIdColorIcon            = 6,
        kIdFileInfo             = 7,    // version 1 only
        kIdFileDatesInfo        = 8,    // version 2 only
        kIdFinderInfo           = 9,
        kIdMacintoshFileInfo    = 10,   // here and below are version 2 only
        kIdProDOSFileInfo       = 11,
        kIdMSDOSFileInfo        = 12,
        kIdShortName            = 13,
        kIdAFPFileInfo          = 14,
        kIdDirectoryId          = 15
    };

    virtual CString Close(void) {
        if (fFp != NULL) {
            fclose(fFp);
            fFp = NULL;
        }
        return L"";
    }
    virtual void XferPrepare(const XferFileOptions* pXferOpts) override
        { ASSERT(false); }
    virtual CString XferFile(LocalFileDetails* pDetails, uint8_t** pDataBuf,
        long dataLen, uint8_t** pRsrcBuf, long rsrcLen) override
        { ASSERT(false); return L"!"; }
    virtual void XferAbort(CWnd* pMsgWnd) override
        { ASSERT(false); }
    virtual void XferFinish(CWnd* pMsgWnd) override
        { ASSERT(false); }

    virtual ArchiveKind GetArchiveKind(void) override { return kArchiveAppleSingle; }
    virtual NuError DoAddFile(const AddFilesDialog* pAddOpts,
        LocalFileDetails* pDetails) override
        { ASSERT(false); return kNuErrGeneric; }


    /*
     * Loads the contents of the archive.
     *
     * Returns 0 on success, < 0 if this is not an AppleSingle file, or
     * > 0 if this appears to be an AppleSingle file but it's damaged.
     */
    int LoadContents();

    /*
     * Confirms that the file is big enough to hold all of the entries
     * listed in the table of contents.
     */
    bool CheckFileLength();

    /*
     * Creates our one and only AppleSingleEntry instance by walking through
     * the various bits of info.
     */
    bool CreateEntry();

    /*
     * Reads the "real name" chunk, converting the character set to
     * Mac OS Roman if necessary.  (If we wanted to be a general AppleSingle
     * tool we wouldn't do that... but we're not.)
     */
    bool HandleRealName(const TOCEntry* tocEntry, AppleSingleEntry* pEntry);

    /*
     * Reads the version 1 File Info chunk, which is OS-specific.  The data
     * layout is determined by the "home file system" string in the header.
     *
     * We only really want to find a ProDOS chunk.  The Macintosh chunk doesn't
     * have the file type in it.
     *
     * This will set the access, file type, aux type, create date/time, and
     * modification date/time.
     */
    bool HandleFileInfo(const TOCEntry* tocEntry, AppleSingleEntry* pEntry);

    /*
     * Reads the version 2 File Dates Info chunk, which provides various
     * dates as 32-bit seconds since Jan 1 2000 UTC.  Nothing else uses
     * this, making it equally inconvenient on all systems.
     */
    bool HandleFileDatesInfo(const TOCEntry* tocEntry,
        AppleSingleEntry* pEntry);

    /*
     * Reads a ProDOS file info block, using the values to set the access,
     * file type, and aux type fields.
     */
    bool HandleProDOSFileInfo(const TOCEntry* tocEntry,
        AppleSingleEntry* pEntry);

    /*
     * Reads a Finder info block, using the values to set the file type and
     * aux type.
     */
    bool HandleFinderInfo(const TOCEntry* tocEntry, AppleSingleEntry* pEntry);

    /*
     * Convert from ProDOS compact date format to time_t (time in seconds
     * since Jan 1 1970 UTC).
     */
    time_t ConvertProDOSDateTime(uint16_t prodosDate, uint16_t prodosTime);

    void DumpArchive();

    FILE*           fFp;
    bool            fIsBigEndian;
    FileHeader      fHeader;
    TOCEntry*       fEntries;
};

#endif /*APP_APPLESINGLEARCHIVE_H*/
