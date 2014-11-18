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

    // retrieve thread data
    virtual int ExtractThreadToBuffer(int which, char** ppText, long* pLength,
        CString* pErrMsg) const;
    virtual int ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
        ConvertHighASCII convHA, CString* pErrMsg) const;
    virtual long GetSelectionSerial(void) const { return -1; }  // doesn't matter

    virtual bool GetFeatureFlag(Feature feature) const {
        if (feature == kFeaturePascalTypes || feature == kFeatureDOSTypes ||
            feature == kFeatureHasSimpleAccess)
            return false;
        else
            return true;
    }

    NuError TestEntry(CWnd* pMsgWnd);

    bool GetSqueezed(void) const { return fIsSqueezed; }
    void SetSqueezed(bool val) { fIsSqueezed = val; }
    long GetOffset(void) const { return fOffset; }
    void SetOffset(long offset) { fOffset = offset; }

    enum {
        kBNYBlockSize       = 128,
    };

private:
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
        CString* pErrMsg);
    virtual CString New(const WCHAR* filename, const void* options);
    virtual CString Flush(void) { return ""; }
    virtual CString Reload(void);
    virtual bool IsReadOnly(void) const { return fIsReadOnly; };
    virtual bool IsModified(void) const { return false; }
    virtual void GetDescription(CString* pStr) const { *pStr = "Binary II"; }
    virtual bool BulkAdd(ActionProgressDialog* pActionProgress,
        const AddFilesDialog* pAddOpts)
        { ASSERT(false); return false; }
    virtual bool AddDisk(ActionProgressDialog* pActionProgress,
        const AddFilesDialog* pAddOpts)
        { ASSERT(false); return false; }
    virtual bool CreateSubdir(CWnd* pMsgWnd, GenericEntry* pParentEntry,
        const WCHAR* newName)
        { ASSERT(false); return false; }
    virtual bool TestSelection(CWnd* pMsgWnd, SelectionSet* pSelSet);
    virtual bool DeleteSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
        { ASSERT(false); return false; }
    virtual bool RenameSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
        { ASSERT(false); return false; }
    virtual bool RenameVolume(CWnd* pMsgWnd, DiskFS* pDiskFS,
        const WCHAR* newName)
        { ASSERT(false); return false; }
    virtual CString TestVolumeName(const DiskFS* pDiskFS,
        const WCHAR* newName) const
        { ASSERT(false); return "!"; }
    virtual CString TestPathName(const GenericEntry* pGenericEntry,
        const CString& basePath, const CString& newName, char newFssep) const
        { ASSERT(false); return "!"; }
    virtual bool RecompressSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
        const RecompressOptionsDialog* pRecompOpts)
        { ASSERT(false); return false; }
    virtual XferStatus XferSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
        ActionProgressDialog* pActionProgress, const XferFileOptions* pXferOpts)
        { ASSERT(false); return kXferFailed; }
    virtual bool GetComment(CWnd* pMsgWnd, const GenericEntry* pEntry,
        CString* pStr)
        { ASSERT(false); return false; }
    virtual bool SetComment(CWnd* pMsgWnd, GenericEntry* pEntry,
        const CString& str)
        { ASSERT(false); return false; }
    virtual bool DeleteComment(CWnd* pMsgWnd, GenericEntry* pEntry)
        { ASSERT(false); return false; }
    virtual bool SetProps(CWnd* pMsgWnd, GenericEntry* pEntry,
        const FileProps* pProps)
        { ASSERT(false); return false; }
    virtual void PreferencesChanged(void) {}
    virtual long GetCapability(Capability cap);

    friend class BnyEntry;

private:
    virtual CString Close(void) {
        if (fFp != NULL) {
            fclose(fFp);
            fFp = NULL;
        }
        return "";
    }
    virtual void XferPrepare(const XferFileOptions* pXferOpts)
        { ASSERT(false); }
    virtual CString XferFile(FileDetails* pDetails, unsigned char** pDataBuf,
        long dataLen, unsigned char** pRsrcBuf, long rsrcLen)
        { ASSERT(false); return "!"; }
    virtual void XferAbort(CWnd* pMsgWnd)
        { ASSERT(false); }
    virtual void XferFinish(CWnd* pMsgWnd)
        { ASSERT(false); }

    virtual ArchiveKind GetArchiveKind(void) { return kArchiveBNY; }
    virtual NuError DoAddFile(const AddFilesDialog* pAddOpts,
        FileDetails* pDetails)
        { ASSERT(false); return kNuErrGeneric; }

    enum {
        kBNYBlockSize       = BnyEntry::kBNYBlockSize,
        kBNYMaxFileName     = 64,
        kBNYMaxNativeName   = 48,
        kBNYFlagCompressed  = (1<<7),
        kBNYFlagEncrypted   = (1<<6),
        kBNYFlagSparse      = (1),
    };

    typedef unsigned char uchar;
    typedef unsigned short ushort;
    typedef unsigned long ulong;

    /*
     * An entry in a Binary II archive.  Each archive is essentially a stream
     * of files; only the "filesToFollow" value gives any indication that
     * something else follows this entry.
     *
     * We read this from the archive and then unpack it into GenericEntry
     * fields in a BnyEntry.
     */
    struct BnyFileEntry;            // VC++6 needs these to access private enums
    friend struct BnyFileEntry;     //   in this class
    typedef struct BnyFileEntry {
        ushort          access;
        ushort          fileType;
        ulong           auxType;
        uchar           storageType;
        ulong           fileSize;           /* in 512-byte blocks */
        ushort          prodosModDate;
        ushort          prodosModTime;
        NuDateTime      modWhen;            /* computed from previous two fields */
        ushort          prodosCreateDate;
        ushort          prodosCreateTime;
        NuDateTime      createWhen;         /* computed from previous two fields */
        ulong           eof;
        ulong           realEOF;            /* eof is bogus for directories */
        char            fileName[kBNYMaxFileName+1];
        char            nativeName[kBNYMaxNativeName+1];
        ulong           diskSpace;          /* in 512-byte blocks */
        uchar           osType;             /* not exactly same as NuFileSysID */
        ushort          nativeFileType;
        uchar           phantomFlag;
        uchar           dataFlags;          /* advisory flags */
        uchar           version;
        uchar           filesToFollow;      /* #of files after this one */

        uchar           blockBuf[kBNYBlockSize];
    } BnyFileEntry;

    int LoadContents(void);
    NuError LoadContentsCallback(BnyFileEntry* pEntry);

    bool IsSqueezed(uchar one, uchar two);
    bool IsDir(BnyFileEntry* pEntry);
    NuError BNYRead(void* buf, size_t nbyte);
    NuError BNYSeek(long offset);
    void BNYConvertDateTime(unsigned short prodosDate,
        unsigned short prodosTime, NuDateTime* pWhen);
    NuError BNYDecodeHeader(BnyFileEntry* pEntry);
    NuError BNYIterate(void);

    FILE*       fFp;
    bool        fIsReadOnly;
};

#endif /*APP_BNYARCHIVE_H*/
