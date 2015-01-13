/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Disk image "archive" support.
 */
#ifndef APP_DISKARCHIVE_H
#define APP_DISKARCHIVE_H

#include "GenericArchive.h"
#include "../diskimg/DiskImg.h"

class RenameEntryDialog;


/*
 * One file in a disk image.
 */
class DiskEntry : public GenericEntry {
public:
    DiskEntry(A2File* pFile) : fpFile(pFile)
        {}
    virtual ~DiskEntry(void) {}

    virtual int ExtractThreadToBuffer(int which, char** ppText, long* pLength,
        CString* pErrMsg) const override;
    virtual int ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
        ConvertHighASCII convHA, CString* pErrMsg) const override;

    virtual long GetSelectionSerial(void) const override
        { return -1; }  // idea: T/S block number

    /*
     * Figure out whether or not we're allowed to change a file's type and
     * aux type.
     */
    virtual bool GetFeatureFlag(Feature feature) const override;

    // return the underlying FS format for this file
    virtual DiskImg::FSFormat GetFSFormat(void) const {
        ASSERT(fpFile != NULL);
        return fpFile->GetFSFormat();
    }

    A2File* GetA2File(void) const { return fpFile; }
    void SetA2File(A2File* pFile) { fpFile = pFile; }

private:
    /*
     * Copy data from the open A2File to outfp, possibly converting EOL along
     * the way.
     */
    DIError CopyData(A2FileDescr* pOpenFile, FILE* outfp, ConvertEOL conv,
        ConvertHighASCII convHA, CString* pMsg) const;

    A2File*     fpFile;
};


/*
 * Disk image add-ons to GenericArchive.
 */
class DiskArchive : public GenericArchive {
public:
    DiskArchive(void) : fpPrimaryDiskFS(NULL), fIsReadOnly(false),
        fpAddDataHead(NULL), fpAddDataTail(NULL)
        {}
    virtual ~DiskArchive(void) { (void) Close(); }

    /* pass this as the "options" value to the New() function */
    typedef struct {
        DiskImgLib::DiskImg::FSFormat       format;
        DiskImgLib::DiskImg::SectorOrder    sectorOrder;
    } NewOptionsBase;
    typedef union {
        NewOptionsBase  base;

        struct {
            NewOptionsBase  base;
            long            numBlocks;
        } blank;
        struct {
            NewOptionsBase  base;
            const WCHAR*    volName;
            long            numBlocks;
        } prodos;
        struct {
            NewOptionsBase  base;
            const WCHAR*    volName;
            long            numBlocks;
        } pascalfs;     // "pascal" is reserved token in MSVC++
        struct {
            NewOptionsBase  base;
            const WCHAR*    volName;
            long            numBlocks;
        } hfs;
        struct {
            NewOptionsBase  base;
            int             volumeNum;
            long            numTracks;
            int             numSectors;
            bool            allocDOSTracks;
        } dos;
    } NewOptions;

    /*
     * Perform one-time initialization of the DiskLib library.
     */
    static CString AppInit(void);

    /*
     * Perform one-time cleanup of DiskImgLib at shutdown time.
     */
    static void AppCleanup(void);

    /*
     * Finish instantiating a DiskArchive object by opening an existing file.
     */
    virtual OpenResult Open(const WCHAR* filename, bool readOnly,
        CString* pErrMsg) override;

    /*
     * Finish instantiating a DiskArchive object by creating a new archive.
     *
     * Returns an error string on failure, or "" on success.
     */
    virtual CString New(const WCHAR* filename, const void* options) override;

    /*
     * Flush the DiskArchive object.
     *
     * Most of the stuff we do with disk images goes straight through, but in
     * the case of compressed disks we don't normally re-compress them until
     * it's time to close them.  This forces us to update the copy on disk.
     *
     * Returns an empty string on success, or an error message on failure.
     */
    virtual CString Flush(void) override;

    /*
     * Reload the stuff from the underlying DiskFS.
     *
     * This also does a "lite" flush of the disk data.  For files that are
     * essentially being written as we go, this does little more than clear
     * the "dirty" flag.  Files that need to be recompressed or have some
     * other slow operation remain dirty.
     *
     * We don't need to do the flush as part of the reload -- we can load the
     * contents with everything in a perfectly dirty state.  We don't need to
     * do it at all.  We do it to keep the "dirty" flag clear when nothing is
     * really dirty, and we do it here because almost all of our functions call
     * "reload" after making changes, which makes it convenient to call from here.
     */
    virtual CString Reload(void) override;

    /*
     * Returns true if the archive has un-flushed modifications pending.
     */
    virtual bool IsModified(void) const override;

    /*
     * Return an description of the disk archive, suitable for display in the
     * main title bar.
     */
    virtual CString GetDescription() const override;

    virtual bool BulkAdd(ActionProgressDialog* pActionProgress,
        const AddFilesDialog* pAddOpts) override;
    virtual bool AddDisk(ActionProgressDialog* pActionProgress,
        const AddFilesDialog* pAddOpts) override
        { ASSERT(false); return false; }
    virtual bool CreateSubdir(CWnd* pMsgWnd, GenericEntry* pParentEntry,
        const WCHAR* newName) override;
    virtual bool TestSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) override
        { ASSERT(false); return false; }
    virtual bool DeleteSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) override;
    virtual bool RenameSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) override;
    virtual CString TestPathName(const GenericEntry* pGenericEntry,
        const CString& basePath, const CString& newName,
        char newFssep) const override;
    virtual bool RenameVolume(CWnd* pMsgWnd, DiskFS* pDiskFS,
        const WCHAR* newName) override;
    virtual CString TestVolumeName(const DiskFS* pDiskFS,
        const WCHAR* newName) const override;
    virtual bool RecompressSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
        const RecompressOptionsDialog* pRecompOpts) override
        { ASSERT(false); return false; }
    virtual bool GetComment(CWnd* pMsgWnd, const GenericEntry* pEntry,
        CString* pStr) override
        { ASSERT(false); return false; }
    virtual bool SetComment(CWnd* pMsgWnd, GenericEntry* pEntry,
        const CString& str) override
        { ASSERT(false); return false; }
    virtual bool DeleteComment(CWnd* pMsgWnd, GenericEntry* pEntry) override
        { ASSERT(false); return false; }
    virtual bool SetProps(CWnd* pMsgWnd, GenericEntry* pEntry,
        const FileProps* pProps) override;

    /*
     * User has updated their preferences.  Take note.
     *
     * Setting preferences in a DiskFS causes those prefs to be pushed down
     * to all sub-volumes.
     */
    virtual void PreferencesChanged(void) override;

    virtual long GetCapability(Capability cap) override;
    virtual XferStatus XferSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
        ActionProgressDialog* pActionProgress,
        const XferFileOptions* pXferOpts) override;

    virtual bool IsReadOnly(void) const { return fIsReadOnly; }
    const DiskImg* GetDiskImg(void) const { return &fDiskImg; }
    DiskFS* GetDiskFS(void) const { return fpPrimaryDiskFS; }

    /*
     * Progress update callback, called from DiskImgLib during read/write
     * operations.
     *
     * Returns "true" if we should continue;
     */
    static bool ProgressCallback(DiskImgLib::A2FileDescr* pFile,
        DiskImgLib::di_off_t max, DiskImgLib::di_off_t current, void* state);

private:
    /*
     * Close the DiskArchive ojbect.
     */
    virtual CString Close(void);

    virtual void XferPrepare(const XferFileOptions* pXferOpts) override;
    virtual CString XferFile(LocalFileDetails* pDetails, uint8_t** pDataBuf,
        long dataLen, uint8_t** pRsrcBuf, long rsrcLen) override;
    virtual void XferAbort(CWnd* pMsgWnd) override;
    virtual void XferFinish(CWnd* pMsgWnd) override;

    /*
     * Progress update callback, called from DiskImgLib while scanning a volume
     * during Open().
     *
     * "str" must not contain a '%'.  (TODO: fix that)
     *
     * Returns "true" if we should continue.
     */
    static bool ScanProgressCallback(void* cookie, const char* str,
        int count);


    /*
     * Internal class used to keep track of files we're adding.
     */
    class FileAddData {
    public:
        FileAddData(const LocalFileDetails* pDetails, char* fsNormalPathMOR) {
            fDetails = *pDetails;

            fFSNormalPathMOR = fsNormalPathMOR;
            fpOtherFork = NULL;
            fpNext = NULL;
        }
        virtual ~FileAddData(void) {}

        FileAddData* GetNext(void) const { return fpNext; }
        void SetNext(FileAddData* pNext) { fpNext = pNext; }
        FileAddData* GetOtherFork(void) const { return fpOtherFork; }
        void SetOtherFork(FileAddData* pData) { fpOtherFork = pData; }

        const LocalFileDetails* GetDetails(void) const { return &fDetails; }

        /*
         * Get the "FS-normal" path, i.e. exactly what we want to appear
         * on the disk image.  This has the result of any conversions, so
         * we need to store it as a narrow Mac OS Roman string.
         */
        const char* GetFSNormalPath(void) const { return fFSNormalPathMOR; }

    private:
        LocalFileDetails fDetails;

        // The DiskFS-normalized version of the storage name.  This is the
        // name as it will appear on the Apple II disk image.
        CStringA        fFSNormalPathMOR;

        FileAddData*    fpOtherFork;
        FileAddData*    fpNext;
    };

    virtual ArchiveKind GetArchiveKind(void) override { return kArchiveDiskImage; }
    virtual NuError DoAddFile(const AddFilesDialog* pAddOpts,
        LocalFileDetails* pDetails) override;

    /*
     * Reload the contents of the archive, showing an error message if the
     * reload fails.
     *
     * Returns 0 on success, -1 on failure.
     */
    int InternalReload(CWnd* pMsgWnd);

    /*
     * Compare DiskEntry display names in descending order (Z-A).
     */
    static int CompareDisplayNamesDesc(const void* ventry1, const void* ventry2);

    /*
     * Load the contents of a "disk archive".  Returns 0 on success.
     */
    int LoadContents(void);

    /*
     * Load the contents of a DiskFS.
     *
     * Recursively handle sub-volumes.  "volName" holds the name of the
     * sub-volume as it should appear in the list.
     */
    int LoadDiskFSContents(DiskFS* pDiskFS, const WCHAR* volName);

    void DowncaseSubstring(CString* pStr, int startPos, int endPos,
        bool prevWasSpace);

    /*
     * Handle a debug message from the DiskImg library.
     */
    static void DebugMsgHandler(const char* file, int line, const char* msg);

    /*
     * A file we're adding clashes with an existing file.  Decide what to do
     * about it.
     *
     * Returns one of the following:
     *  kNuOverwrite - overwrite the existing file
     *  kNuSkip - skip adding the existing file
     *  kNuRename - user wants to rename the file
     *  kNuAbort - cancel out of the entire add process
     *
     * Side effects:
     *  Sets fOverwriteExisting and fOverwriteNoAsk if a "to all" button is hit
     *  Replaces pDetails->storageName if the user elects to rename
     */
    NuResult HandleReplaceExisting(const A2File* pExisting,
        LocalFileDetails* pDetails);

    /*
     * Process the list of pending file adds.
     *
     * This is where the rubber (finally!) meets the road.
     */
    CString ProcessFileAddData(DiskFS* pDiskFS, int addOptsConvEOL);

    /*
     * Load a file into a buffer, possibly converting EOL markers and setting
     * "high ASCII" along the way.
     *
     * Returns a pointer to a newly-allocated buffer (new[]) and the data length.
     * If the file is empty, no buffer will be allocated.
     *
     * Returns an empty string on success, or an error message on failure.
     */
    CString LoadFile(const WCHAR* pathName, uint8_t** pBuf, long* pLen,
        GenericEntry::ConvertEOL conv, GenericEntry::ConvertHighASCII convHA) const;

    /*
     * Add a file with the supplied data to the disk image.
     *
     * Forks that exist but are empty have a length of zero.  Forks that don't
     * exist have a length of -1.
     *
     * Called by XferFile and ProcessFileAddData.
     */
    DIError AddForksToDisk(DiskFS* pDiskFS, const DiskFS::CreateParms* pParms,
        const uint8_t* dataBuf, long dataLen,
        const uint8_t* rsrcBuf, long rsrcLen) const;

    /*
     * Add an entry to the end of the FileAddData list.
     *
     * If "storageName" (the Windows filename with type goodies stripped, but
     * without filesystem normalization) matches an entry already in the list,
     * we check to see if these are forks of the same file.  If they are
     * different forks and we don't already have both forks, we put the
     * pointer into the "fork pointer" of the existing file rather than adding
     * it to the end of the list.
     */
    void AddToAddDataList(FileAddData* pData);

    /*
     * Free all entries in the FileAddData list.
     */
    void FreeAddDataList(void);

    /*
     * Set up a RenameEntryDialog for the entry in "*pEntry".
     *
     * Returns true on success, false on failure.
     */
    bool SetRenameFields(CWnd* pMsgWnd, DiskEntry* pEntry,
        RenameEntryDialog* pDialog);

    DiskImg         fDiskImg;           // DiskImg object for entire disk
    DiskFS*         fpPrimaryDiskFS;    // outermost DiskFS
    bool            fIsReadOnly;

    /* active state while adding files */
    FileAddData*    fpAddDataHead;
    FileAddData*    fpAddDataTail;
    bool            fOverwriteExisting;
    bool            fOverwriteNoAsk;

    /* state during xfer */
    //CString           fXferStoragePrefix;
    DiskFS*         fpXferTargetFS;
};

#endif /*APP_DISKARCHIVE_H*/
