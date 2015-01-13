/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * NuFX archive support.
 */
#ifndef APP_NUFXARCHIVE_H
#define APP_NUFXARCHIVE_H

#include "GenericArchive.h"
#include "../nufxlib/NufxLib.h"        // ideally this wouldn't be here, only in .cpp


/*
 * One file in an NuFX archive.
 */
class NufxEntry : public GenericEntry {
public:
    NufxEntry(NuArchive* pArchive) : fpArchive(pArchive)
        {}
    virtual ~NufxEntry(void) {}

    NuRecordIdx GetRecordIdx(void) const { return fRecordIdx; }
    void SetRecordIdx(NuRecordIdx idx) { fRecordIdx = idx; }

    virtual int ExtractThreadToBuffer(int which, char** ppText, long* pLength,
        CString* pErrMsg) const override;
    virtual int ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
        ConvertHighASCII convHA, CString* pErrMsg) const override;

    virtual long GetSelectionSerial(void) const override { return fRecordIdx; }

    virtual bool GetFeatureFlag(Feature feature) const override {
        if (feature == kFeaturePascalTypes || feature == kFeatureDOSTypes ||
            feature == kFeatureHasSimpleAccess)
        {
            return false;
        } else {
            return true;
        }
    }

    /*
     * Analyzes the contents of a record to determine if it's a disk, file,
     * or "other".  Computes the total compressed and uncompressed lengths
     * of all data threads.  Fills out several GenericEntry fields.
     */
    void AnalyzeRecord(const NuRecord* pRecord);

    friend class NufxArchive;

private:
    /*
     * Find info for the thread we're about to extract.
     *
     * Given the NuRecordIdx stored in the object, find the thread whose
     * ThreadID matches "which".  Copies the NuThread structure into
     * "*pThread".
     *
     * On entry *pErrMsg must be an empty string.  On failure, it will
     * contain an error message describing the problem.
     */
    void FindThreadInfo(int which, NuThread* pThread, CString* pErrMsg) const;

    NuRecordIdx fRecordIdx; // unique record index
    NuArchive*  fpArchive;
};


/*
 * A generic archive plus NuFX-specific goodies.
 */
class NufxArchive : public GenericArchive {
public:
    NufxArchive(void) :
        fpArchive(NULL),
        fIsReadOnly(false),
        fProgressAsRecompress(false),
        fNumAdded(-1),
        fpMsgWnd(NULL),
        fpAddOpts(NULL)
    {}
    virtual ~NufxArchive(void) { (void) Close(); }

    /*
     * Perform one-time initialization of the NufxLib library.
     *
     * Returns with an error if the NufxLib version is off.  Major version must
     * match (since it indicates an interface change), minor version must be
     * >= what we expect (in case we're relying on recent behavior changes).
     *
     * Returns 0 on success, nonzero on error.
     */
    static CString AppInit(void);

    /*
     * Finish instantiating a NufxArchive object by opening an existing file.
     */
    virtual OpenResult Open(const WCHAR* filename, bool readOnly,
        CString* pErrMsg) override;

    /*
     * Finish instantiating a NufxArchive object by creating a new archive.
     *
     * Returns an error string on failure, or "" on success.
     */
    virtual CString New(const WCHAR* filename, const void* options) override;

    virtual CString Flush(void) override { return ""; }
    virtual CString Reload(void) override;
    virtual bool IsReadOnly(void) const override { return fIsReadOnly; };
    virtual bool IsModified(void) const override { return false; }
    virtual CString GetDescription() const override { return L"NuFX"; }
    virtual bool BulkAdd(ActionProgressDialog* pActionProgress,
        const AddFilesDialog* pAddOpts) override;
    virtual bool AddDisk(ActionProgressDialog* pActionProgress,
        const AddFilesDialog* pAddOpts) override;
    virtual bool CreateSubdir(CWnd* pMsgWnd, GenericEntry* pParentEntry,
        const WCHAR* newName) override
        { ASSERT(false); return false; }
    virtual bool TestSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) override;
    virtual bool DeleteSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) override;
    virtual bool RenameSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) override;
    virtual CString TestPathName(const GenericEntry* pGenericEntry,
        const CString& basePath, const CString& newName,
        char newFssep) const override;
    virtual bool RenameVolume(CWnd* pMsgWnd, DiskFS* pDiskFS,
        const WCHAR* newName) override
        { ASSERT(false); return false; }
    virtual CString TestVolumeName(const DiskFS* pDiskFS,
        const WCHAR* newName) const override
        { ASSERT(false); return L"!"; }
    virtual bool RecompressSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
        const RecompressOptionsDialog* pRecompOpts) override;
    virtual XferStatus XferSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
        ActionProgressDialog* pActionProgress,
        const XferFileOptions* pXferOpts) override;
    virtual bool GetComment(CWnd* pMsgWnd, const GenericEntry* pEntry,
        CString* pStr) override;
    virtual bool SetComment(CWnd* pMsgWnd, GenericEntry* pEntry,
        const CString& str) override;
    virtual bool DeleteComment(CWnd* pMsgWnd, GenericEntry* pEntry) override;
    virtual bool SetProps(CWnd* pMsgWnd, GenericEntry* pEntry,
        const FileProps* pProps) override;
    virtual void PreferencesChanged(void) override;
    virtual long GetCapability(Capability cap) override;

    // try not to use this
    NuArchive* GetNuArchivePointer(void) const { return fpArchive; }

    // determine whether a particular type of compression is supported
    static bool IsCompressionSupported(NuThreadFormat format);

    // convert from DateTime format to time_t
    static time_t DateTimeToSeconds(const NuDateTime* pDateTime);

private:
    virtual CString Close(void) {
        if (fpArchive != NULL) {
            LOGI("Closing archive (aborting any un-flushed changes)");
            NuAbort(fpArchive);
            NuClose(fpArchive);
            fpArchive = NULL;
        }
        return L"";
    }

    // recompress one thread
    bool RecompressThread(NufxEntry* pEntry, int threadKind,
        const RecompressOptionsDialog* pRecompOpts, long* pSizeInMemory,
        CString* pErrMsg);

    virtual void XferPrepare(const XferFileOptions* pXferOpts) override;
    virtual CString XferFile(LocalFileDetails* pDetails, uint8_t** pDataBuf,
        long dataLen, uint8_t** pRsrcBuf, long rsrcLen) override;
    virtual void XferAbort(CWnd* pMsgWnd) override;
    virtual void XferFinish(CWnd* pMsgWnd) override;

    virtual ArchiveKind GetArchiveKind(void) override { return kArchiveNuFX; }

    // prepare to add files
    void AddPrep(CWnd* pWnd, const AddFilesDialog* pAddOpts);

    /*
     * Reset some things after we finish adding files.  We don't necessarily
     * want these to stay in effect for other operations, e.g. extracting.
     */
    void AddFinish(void);

    virtual NuError DoAddFile(const AddFilesDialog* pAddOpts,
        LocalFileDetails* pDetails) override;

    /*
     * Error handler callback for "bulk" adds.
     */
    static NuResult BulkAddErrorHandler(NuArchive* pArchive, void* vErrorStatus);

    /*
     * Decide whether or not to replace an existing file (during extract)
     * or record (during add).
     */
    NuResult HandleReplaceExisting(const NuErrorStatus* pErrorStatus);

    /*
     * A file that used to be there isn't anymore.
     *
     * This should be exceedingly rare.
     */
    NuResult HandleAddNotFound(const NuErrorStatus* pErrorStatus);

    /*
     * Load the contents of an archive into the GenericEntry/NufxEntry list.
     */
    NuError LoadContents(void);

    /*
     * Reload the contents of the archive, showing an error message if the
     * reload fails.
     */
    NuError InternalReload(CWnd* pMsgWnd);

    /*
     * Static callback function.  Used for scanning the contents of an archive.
     */
    static NuResult ContentFunc(NuArchive* pArchive, void* vpRecord);

    /*
     * Set some standard callbacks and feature flags.
     */
    NuError SetCallbacks(void);

    // handle progress update messages
    static NuResult ProgressUpdater(NuArchive* pArchive, void* vpProgress);

    // handle error and debug messages from NufxLib.
    static NuResult NufxErrorMsgHandler(NuArchive* pArchive,
        void* vErrorMessage);

    // handle a DataSource resource release request; used for memory allocated
    // with new[]
    static NuResult ArrayDeleteHandler(NuArchive* pArchive, void* ptr);

    NuArchive*      fpArchive;
    bool            fIsReadOnly;

    bool            fProgressAsRecompress;  // tweak progress updater

    /* state while adding files */
    int             fNumAdded;
    CWnd*           fpMsgWnd;
    const AddFilesDialog*   fpAddOpts;
};

#endif /*APP_NUFXARCHIVE_H*/
