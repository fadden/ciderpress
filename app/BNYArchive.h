/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Binary II support.
 */
#ifndef APP_BNYARCHIVE_H
#define APP_BNYARCHIVE_H

#include "GenericArchive.h"


class BnyArchive;

/*
 * One file in a BNY archive.
 */
class BnyEntry : public GenericEntry {
public:
    BnyEntry(BnyArchive* pArchive) :
        fpArchive(pArchive), fIsSqueezed(false), fOffset(-1)
        {}
    virtual ~BnyEntry(void) {}

    virtual int ExtractThreadToBuffer(int which, char** ppText, long* pLength,
        CString* pErrMsg) const override;
    virtual int ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
        ConvertHighASCII convHA, CString* pErrMsg) const override;

    virtual long GetSelectionSerial(void) const override
        { return -1; }  // doesn't matter

    virtual bool GetFeatureFlag(Feature feature) const override {
        if (feature == kFeaturePascalTypes || feature == kFeatureDOSTypes ||
            feature == kFeatureHasSimpleAccess)
            return false;
        else
            return true;
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

    enum {
        kBNYBlockSize       = 128,
    };

private:
    /*
     * Copy data from the seeked archive to outfp, possibly converting EOL along
     * the way.
     */
    NuError CopyData(FILE* outfp, ConvertEOL conv, ConvertHighASCII convHA,
        CString* pMsg) const;
    //NuError BNYUnSqueeze(ExpandBuffer* outExp) const;

    BnyArchive* fpArchive;      // holds FILE* for archive
    bool        fIsSqueezed;
    long        fOffset;
};


/*
 * BNY archive definition.
 */
class BnyArchive : public GenericArchive {
public:
    BnyArchive(void) : fIsReadOnly(false), fFp(NULL)
        {}
    virtual ~BnyArchive(void) { (void) Close(); }

    // One-time initialization; returns an error string.
    static CString AppInit(void);

    virtual OpenResult Open(const WCHAR* filename, bool readOnly,
        CString* pErrMsg) override;
    virtual CString New(const WCHAR* filename, const void* options) override;
    virtual CString Flush(void) override { return ""; }
    virtual CString Reload(void) override;
    virtual bool IsReadOnly(void) const override { return fIsReadOnly; };
    virtual bool IsModified(void) const override { return false; }
    virtual CString GetDescription() const override { return L"Binary II"; }
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
        { ASSERT(false); return "!"; }
    virtual CString TestPathName(const GenericEntry* pGenericEntry,
        const CString& basePath, const CString& newName,
        char newFssep) const override
        { ASSERT(false); return "!"; }
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

    friend class BnyEntry;

private:
    virtual CString Close(void) {
        if (fFp != NULL) {
            fclose(fFp);
            fFp = NULL;
        }
        return "";
    }
    virtual void XferPrepare(const XferFileOptions* pXferOpts) override
        { ASSERT(false); }
    virtual CString XferFile(LocalFileDetails* pDetails, uint8_t** pDataBuf,
        long dataLen, uint8_t** pRsrcBuf, long rsrcLen) override
        { ASSERT(false); return "!"; }
    virtual void XferAbort(CWnd* pMsgWnd) override
        { ASSERT(false); }
    virtual void XferFinish(CWnd* pMsgWnd) override
        { ASSERT(false); }

    virtual ArchiveKind GetArchiveKind(void) override { return kArchiveBNY; }
    virtual NuError DoAddFile(const AddFilesDialog* pAddOpts,
        LocalFileDetails* pDetails) override
        { ASSERT(false); return kNuErrGeneric; }

    enum {
        kBNYBlockSize       = BnyEntry::kBNYBlockSize,
        kBNYMaxFileName     = 64,
        kBNYMaxNativeName   = 48,
        kBNYFlagCompressed  = (1<<7),
        kBNYFlagEncrypted   = (1<<6),
        kBNYFlagSparse      = (1),
    };

    /*
     * An entry in a Binary II archive.  Each archive is essentially a stream
     * of files; only the "filesToFollow" value gives any indication that
     * something else follows this entry.
     *
     * We read this from the archive and then unpack it into GenericEntry
     * fields in a BnyEntry.
     */
//    struct BnyFileEntry;            // VC++6 needs these to access private enums
//    friend struct BnyFileEntry;     //   in this class
    typedef struct BnyFileEntry {
        uint16_t        access;
        uint16_t        fileType;
        uint32_t        auxType;
        uint8_t         storageType;
        uint32_t        fileSize;           /* in 512-byte blocks */
        uint16_t        prodosModDate;
        uint16_t        prodosModTime;
        NuDateTime      modWhen;            /* computed from previous two fields */
        uint16_t         prodosCreateDate;
        uint16_t        prodosCreateTime;
        NuDateTime      createWhen;         /* computed from previous two fields */
        uint32_t        eof;
        uint32_t        realEOF;            /* eof is bogus for directories */
        char            fileName[kBNYMaxFileName+1];
        char            nativeName[kBNYMaxNativeName+1];
        uint32_t        diskSpace;          /* in 512-byte blocks */
        uint8_t         osType;             /* not exactly same as NuFileSysID */
        uint16_t        nativeFileType;
        uint8_t         phantomFlag;
        uint8_t         dataFlags;          /* advisory flags */
        uint8_t         version;
        uint8_t         filesToFollow;      /* #of files after this one */

        uint8_t         blockBuf[kBNYBlockSize];
    } BnyFileEntry;

    int LoadContents(void);

    /*
     * Given a BnyFileEntry structure, add an appropriate entry to the list.
     *
     * Note this can mangle pEntry (notably the filename).
     */
    NuError LoadContentsCallback(BnyFileEntry* pEntry);

    /*
     * Test for the magic number on a file in SQueezed format.
     */
    bool IsSqueezed(uint8_t one, uint8_t two);

    /*
     * Test if this entry is a directory.
     */
    bool IsDir(BnyFileEntry* pEntry);

    /*
     * Wrapper for fread().  Note the arguments resemble read(2) rather
     * than fread(3S).
     */
    NuError BNYRead(void* buf, size_t nbyte);

    /*
     * Seek within an archive.  Because we need to handle streaming archives,
     * and don't need to special-case anything, we only allow relative
     * forward seeks.
     */
    NuError BNYSeek(long offset);

    /*
     * Convert from ProDOS compact date format to the expanded DateTime format.
     */
    void BNYConvertDateTime(unsigned short prodosDate,
        unsigned short prodosTime, NuDateTime* pWhen);

    /*
     * Decode a Binary II header.
     */
    NuError BNYDecodeHeader(BnyFileEntry* pEntry);

    /*
     * Iterate through a Binary II archive, loading the data.
     */
    NuError BNYIterate(void);

    FILE*       fFp;
    bool        fIsReadOnly;
};

#endif /*APP_BNYARCHIVE_H*/
