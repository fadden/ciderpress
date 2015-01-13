/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * AppleLink Compression Utility archive support (read-only).
 */
#ifndef APP_ACUARCHIVE_H
#define APP_ACUARCHIVE_H

#include "GenericArchive.h"


class AcuArchive;

/*
 * One file in an ACU archive.
 */
class AcuEntry : public GenericEntry {
public:
    AcuEntry(AcuArchive* pArchive) :
        fpArchive(pArchive), fIsSqueezed(false), fOffset(-1)
        {}
    virtual ~AcuEntry(void) {}

    virtual int ExtractThreadToBuffer(int which, char** ppText, long* pLength,
        CString* pErrMsg) const override;
    virtual int ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
        ConvertHighASCII convHA, CString* pErrMsg) const override;

    // doesn't matter
    virtual long GetSelectionSerial(void) const override { return -1; }

    virtual bool GetFeatureFlag(Feature feature) const override {
        if (feature == kFeatureHasFullAccess ||
            feature == kFeatureCanChangeType ||
            feature == kFeatureHasInvisibleFlag)
        {
            return true;
        } else {
            return false;
        }
    }

    /*
     * Test this entry by extracting it.
     *
     * If the file isn't compressed, just make sure the file is big enough.  If
     * it's squeezed, invoke the un-squeeze function with a "NULL" buffer pointer.
     */
    NuError TestEntry(CWnd* pMsgWnd);

    bool GetSqueezed(void) const { return fIsSqueezed; }
    void SetSqueezed(bool val) { fIsSqueezed = val; }
    long GetOffset(void) const { return fOffset; }
    void SetOffset(long offset) { fOffset = offset; }

private:
    /*
     * Copy data from the seeked archive to outfp, possibly converting EOL along
     * the way.
     */
    NuError CopyData(FILE* outfp, ConvertEOL conv, ConvertHighASCII convHA,
        CString* pMsg) const;

    AcuArchive* fpArchive;      // holds FILE* for archive
    bool        fIsSqueezed;
    long        fOffset;
};


/*
 * ACU archive definition.
 */
class AcuArchive : public GenericArchive {
public:
    AcuArchive(void) : fFp(NULL) {}
    virtual ~AcuArchive(void) { (void) Close(); }

    /*
     * Perform one-time initialization.  There really isn't any for us.
     *
     * Returns an error string on failure.
     */
    static CString AppInit(void);

    /*
     * Open an ACU archive.
     *
     * Returns an error string on failure, or "" on success.
     */
    virtual OpenResult Open(const WCHAR* filename, bool readOnly,
        CString* pErrMsg) override;

    /*
     * Finish instantiating an AcuArchive object by creating a new archive.
     *
     * This isn't implemented, and will always return an error.
     */
    virtual CString New(const WCHAR* filename, const void* options) override;

    virtual CString Flush(void) override { return L""; }

    virtual CString Reload(void) override;
    virtual bool IsReadOnly(void) const override { return true; };
    virtual bool IsModified(void) const override { return false; }
    virtual CString GetDescription() const override { return L"AppleLink ACU"; }
    virtual bool BulkAdd(ActionProgressDialog* pActionProgress,
        const AddFilesDialog* pAddOpts) override
        { ASSERT(false); return false; }
    virtual bool AddDisk(ActionProgressDialog* pActionProgress,
        const AddFilesDialog* pAddOpts) override
        { ASSERT(false); return false; }
    virtual bool CreateSubdir(CWnd* pMsgWnd, GenericEntry* pParentEntry,
        const WCHAR* newName) override
        { ASSERT(false); return false; }
    virtual bool TestSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) override;
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

    friend class AcuEntry;

private:
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

    virtual ArchiveKind GetArchiveKind(void) override { return kArchiveACU; }
    virtual NuError DoAddFile(const AddFilesDialog* pAddOpts,
        LocalFileDetails* pDetails) override
        { ASSERT(false); return kNuErrGeneric; }

    enum {
        kAcuMaxFileName     = 256,      // nice big number

        kAcuMasterHeaderLen = 20,
        kAcuEntryHeaderLen  = 54,
    };

    /*
     * The header at the front of an ACU archive.
     */
    struct AcuMasterHeader {
        uint16_t    fileCount;
        uint16_t    unknown1;       // 0x01 00 -- might be "version 1?"
        uint8_t     fZink[6];       // "fZink", low ASCII
        uint8_t     unknown2[11];   // 0x01 36 00 00 00 00 00 00 00 00 dd
    };

    /*
     * An entry in an ACU archive.  Each archive is essentially a stream
     * of files; only the "filesToFollow" value gives any indication that
     * something else follows this entry.
     *
     * We read this from the archive and then unpack the interesting parts
     * into GenericEntry fields in an AcuEntry.
     */
    //struct AcuFileEntry;
    //friend struct AcuFileEntry;
    struct AcuFileEntry {
        uint8_t     compressionType;
        uint16_t    dataChecksum;       // ??
        uint16_t    blockCount;         // total blocks req'd to hold file
        uint32_t    dataStorageLen;     // length of data within archive
        uint16_t    access;
        uint16_t    fileType;
        uint32_t    auxType;
        uint8_t     storageType;
        uint32_t    dataEof;
        uint16_t    prodosModDate;
        uint16_t    prodosModTime;
        NuDateTime  modWhen;            // computed from previous two fields
        uint16_t    prodosCreateDate;
        uint16_t    prodosCreateTime;
        NuDateTime  createWhen;         // computed from previous two fields
        uint16_t    fileNameLen;
        uint16_t    headerChecksum;     // ??
        char        fileName[kAcuMaxFileName+1];    // ASCII

        // possibilities for mystery fields:
        // - OS type (note ProDOS is $00)
        // - forked file support
    };

    /* known compression types */
    enum CompressionType {
        kAcuCompNone = 0,
        kAcuCompSqueeze = 3,
    };

    /*
     * Load the contents of the archive.
     *
     * Returns 0 on success, < 0 if this is not an ACU archive, or > 0 if
     * this appears to be an ACU archive but it's damaged.
     */
    int LoadContents(void);

    /*
     * Read the archive header.  The archive file is left seeked to the point
     * at the end of the header.
     *
     * Returns 0 on success, -1 on failure.  Sets *pNumEntries to the number of
     * entries in the archive.
     */
    int ReadMasterHeader(int* pNumEntries);

    /*
     * Read and decode an AppleLink Compression Utility file entry header.
     * This leaves the file seeked to the point immediately past the filename.
     */
    NuError ReadFileHeader(AcuFileEntry* pEntry);

    /*
     * Dump the contents of an AcuFileEntry struct.
     */
    void DumpFileHeader(const AcuFileEntry* pEntry);

    /*
     * Given an AcuFileEntry structure, add an appropriate entry to the list.
     */
    int CreateEntry(const AcuFileEntry* pEntry);

    /*
     * Test if this entry is a directory.
     */
    bool IsDir(const AcuFileEntry* pEntry);

    /*
     * Wrapper for fread().  Note the arguments resemble read(2) rather
     * than fread(3S).
     */
    NuError AcuRead(void* buf, size_t nbyte);

    /*
     * Seek within an archive.  Because we need to handle streaming archives,
     * and don't need to special-case anything, we only allow relative
     * forward seeks.
     */
    NuError AcuSeek(long offset);

    /*
     * Convert from ProDOS compact date format to the expanded DateTime format.
     */
    void AcuConvertDateTime(uint16_t prodosDate,
        uint16_t prodosTime, NuDateTime* pWhen);

    FILE*       fFp;
    //bool        fIsReadOnly;
};

#endif /*APP_ACUARCHIVE_H*/
