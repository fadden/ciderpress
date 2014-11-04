/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Generic Apple II archive handling.
 *
 * These are abstract base classes.
 */
#ifndef __GENERIC_ARCHIVE__
#define __GENERIC_ARCHIVE__

#include "Preferences.h"
#include "../util/UtilLib.h"
#include "../diskimg/DiskImg.h"
#include "../prebuilt/NufxLib.h"
#include "../reformat/Reformat.h"
#include <time.h>
#include <string.h>

// this shouldn't be in a header file, but in here it's probably okay
using namespace DiskImgLib;

class ActionProgressDialog;
class AddFilesDialog;
class RecompressOptionsDialog;
class SelectionSet;
struct Win32dirent;
class GenericArchive;

const int kFileTypeTXT = 0x04;
const int kFileTypeBIN = 0x06;
const int kFileTypeSRC = 0xb0;
const int kFileTypeINT = 0xfa;
const int kFileTypeBAS = 0xfc;

/*
 * Set of data allowed in file property "set file info" calls.
 */
typedef struct FileProps {
    unsigned long   fileType;
    unsigned long   auxType;
    unsigned long   access;
    time_t          createWhen;
    time_t          modWhen;
} FileProps;

/*
 * Options for converting between file archives and disk archives.
 */
class XferFileOptions {
public:
    XferFileOptions(void) :
        fTarget(nil), fPreserveEmptyFolders(false), fpTargetFS(nil)
        {}
    ~XferFileOptions(void) {}

    /* where the stuff is going */
    GenericArchive* fTarget;

    /* these really only have meaning when converting disk to file */
    //bool  fConvDOSText;
    //bool  fConvPascalText;
    bool    fPreserveEmptyFolders;

    /* only useful when converting files to a disk image */
    //CString   fStoragePrefix;
    DiskImgLib::DiskFS* fpTargetFS;
};


/*
 * Generic description of an Apple II file.
 *
 * Everything returned by the basic "get" calls for display in the ContentList
 * must be held in local storage (i.e. not just pointers into DiskFS data).
 * Otherwise, we run the risk of doing some DiskFS updates and having weird
 * things happen in the ContentList.
 */
class GenericEntry {
public:
    GenericEntry(void);
    virtual ~GenericEntry(void);

    /* kinds of files found in archives */
    enum RecordKind {
        kRecordKindUnknown = 0,
        kRecordKindDisk,
        kRecordKindFile,
        kRecordKindForkedFile,
        kRecordKindDirectory,
        kRecordKindVolumeDir,
    };
    /*
     * Threads we will view or extract (threadMask).  This is no longer used
     * for viewing files, but still plays a role when extracting.
     */
    enum {
        // create one entry for each matching thread
        kDataThread = 0x01,
        kRsrcThread = 0x02,
        kDiskImageThread = 0x04,
        kCommentThread = 0x08,

        // grab any of the above threads
        kAnyThread = 0x10,

        // set this if we allow matches on directory entries
        kAllowDirectory = 0x20,

        // and volume directory entries
        kAllowVolumeDir = 0x40,

        // set to include "damaged" files
        kAllowDamaged = 0x80,
    };
    /* EOL conversion mode for threads being extracted */
    typedef enum ConvertEOL {
        kConvertUnknown = 0, kConvertEOLOff, kConvertEOLOn, kConvertEOLAuto
    } ConvertEOL;
    typedef enum EOLType {
        kEOLUnknown = 0, kEOLCR, kEOLLF, kEOLCRLF
    };
    /* high ASCII conversion mode for threads being extracted */
    typedef enum ConvertHighASCII {
        kConvertHAUnknown = 0, kConvertHAOff, kConvertHAOn, kConvertHAAuto
    } ConvertHighASCII;

    /* ProDOS access flags, used for all filesystems */
    enum {
        kAccessRead         = 0x01,
        kAccessWrite        = 0x02,
        kAccessInvisible    = 0x04,
        kAccessBackup       = 0x20,
        kAccessRename       = 0x40,
        kAccessDelete       = 0x80
    };

    /* features supported by underlying archive */
    typedef enum Feature {
        kFeatureCanChangeType,
        kFeaturePascalTypes,
        kFeatureDOSTypes,
        kFeatureHFSTypes,
        kFeatureHasFullAccess,
        kFeatureHasSimpleAccess,    // mutually exclusive with FullAccess
        kFeatureHasInvisibleFlag,
    } Feature;

    // retrieve thread (or filesystem) data
    virtual int ExtractThreadToBuffer(int which, char** ppText, long* pLength,
        CString* pErrMsg) const = 0;
    virtual int ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
        ConvertHighASCII convHA, CString* pErrMsg) const = 0;

    // This helps us retain the ContentList selection across a Reload().  Only
    // necessary for read-write archives, since those are the only ones that
    // ever need to be reloaded.  Value must be nonzero to be used.
    virtual long GetSelectionSerial(void) const = 0;

    /* are we allowed to change the file/aux type of this entry? */
    /* (may need to generalize this to "changeable attrs" bitmask) */
    virtual bool GetFeatureFlag(Feature feature) const = 0;

    long GetIndex(void) const { return fIndex; }
    void SetIndex(long idx) { fIndex = idx; }

    const char* GetPathName(void) const { return fPathName; }
    void SetPathName(const char* path);
    const char* GetFileName(void);
    const char* GetFileNameExtension(void); // returns e.g. ".SHK"
    void SetSubVolName(const char* name);
    const char* GetSubVolName(void) const { return fSubVolName; }
    const char* GetDisplayName(void) const; // not really "const"

    char GetFssep(void) const { return fFssep; }
    void SetFssep(char fssep) { fFssep = fssep; }
    long GetFileType(void) const { return fFileType; }
    void SetFileType(long type) { fFileType = type; }
    long GetAuxType(void) const { return fAuxType; }
    void SetAuxType(long type) { fAuxType = type; }
    long GetAccess(void) const { return fAccess; }
    void SetAccess(long access) { fAccess = access; }
    time_t GetCreateWhen(void) const { return fCreateWhen; }
    void SetCreateWhen(time_t when) { fCreateWhen = when; }
    time_t GetModWhen(void) const { return fModWhen; }
    void SetModWhen(time_t when) { fModWhen = when; }
    RecordKind GetRecordKind(void) const { return fRecordKind; }
    void SetRecordKind(RecordKind recordKind) { fRecordKind = recordKind; }
    const char* GetFormatStr(void) const { return fFormatStr; }
    void SetFormatStr(const char* str) { fFormatStr = str; } // arg not copied, must be static!
    LONGLONG GetCompressedLen(void) const { return fCompressedLen; }
    void SetCompressedLen(LONGLONG len) { fCompressedLen = len; }
    LONGLONG GetUncompressedLen(void) const {
        return fDataForkLen + fRsrcForkLen;
    }
    //void SetUncompressedLen(LONGLONG len) { fUncompressedLen = len; }
    LONGLONG GetDataForkLen(void) const { return fDataForkLen; }
    void SetDataForkLen(LONGLONG len) { fDataForkLen = len; }
    LONGLONG GetRsrcForkLen(void) const { return fRsrcForkLen; }
    void SetRsrcForkLen(LONGLONG len) { fRsrcForkLen = len; }

    DiskImg::FSFormat GetSourceFS(void) const { return fSourceFS; }
    void SetSourceFS(DiskImg::FSFormat fmt) { fSourceFS = fmt; }

    bool GetHasDataFork(void) const { return fHasDataFork; }
    void SetHasDataFork(bool val) { fHasDataFork = val; }
    bool GetHasRsrcFork(void) const { return fHasRsrcFork; }
    void SetHasRsrcFork(bool val) { fHasRsrcFork = val; }
    bool GetHasDiskImage(void) const { return fHasDiskImage; }
    void SetHasDiskImage(bool val) { fHasDiskImage = val; }
    bool GetHasComment(void) const { return fHasComment; }
    void SetHasComment(bool val) { fHasComment = val; }
    bool GetHasNonEmptyComment(void) const { return fHasNonEmptyComment; }
    void SetHasNonEmptyComment(bool val) { fHasNonEmptyComment = val; }

    bool GetDamaged(void) const { return fDamaged; }
    void SetDamaged(bool val) { fDamaged = val; }
    bool GetSuspicious(void) const { return fSuspicious; }
    void SetSuspicious(bool val) { fSuspicious = val; }

    GenericEntry* GetPrev(void) const { return fpPrev; }
    void SetPrev(GenericEntry* pEntry) { fpPrev = pEntry; }
    GenericEntry* GetNext(void) const { return fpNext; }
    void SetNext(GenericEntry* pEntry) { fpNext = pEntry; }

    // Utility functions.
    const char* GetFileTypeString(void) const;
    static bool CheckHighASCII(const unsigned char* buffer,
        unsigned long count);

    static ConvertEOL DetermineConversion(const unsigned char* buffer,
        long count, EOLType* pSourceType, ConvertHighASCII* pConvHA);
    static int GenericEntry::WriteConvert(FILE* fp, const char* buf,
        size_t len, ConvertEOL* pConv, ConvertHighASCII* pConvHA,
        bool* pLastCR);

protected:
    static void SpacesToUnderscores(char* buf);

private:
    char*       fPathName;
    const char* fFileName;          // points within fPathName
    const char* fFileNameExtension; // points within fPathName
    char        fFssep;
    char*       fSubVolName;        // sub-volume prefix, or nil if none
    char*       fDisplayName;       // combination of sub-vol and path
    long        fFileType;
    long        fAuxType;
    long        fAccess;
    time_t      fCreateWhen;
    time_t      fModWhen;
    RecordKind  fRecordKind;        // forked file, disk image, ??
    const char* fFormatStr;         // static str; compression or fs format
    //LONGLONG  fUncompressedLen;
    LONGLONG    fDataForkLen;       // also for disk images
    LONGLONG    fRsrcForkLen;       // set to 0 when nonexistent
    LONGLONG    fCompressedLen;     // data/disk + rsrc
    
    DiskImg::FSFormat fSourceFS;    // if DOS3.3, text files have funky format

    bool        fHasDataFork;
    bool        fHasRsrcFork;
    bool        fHasDiskImage;
    bool        fHasComment;
    bool        fHasNonEmptyComment; // set if fHasComment and it isn't empty

    bool        fDamaged;           // if set, don't try to open file
    bool        fSuspicious;        // if set, file *might* be damaged

    long        fIndex;         // serial index, for sorting "unsorted" view
    GenericEntry* fpPrev;
    GenericEntry* fpNext;
};

/*
 * Generic representation of a collection of Apple II files.
 *
 * This raises the "reload" flag whenever its data is reloaded.  Any code that
 * keeps pointers to stuff (e.g. local copies of pointers to GenericEntry
 * objects) needs to check the "reload" flag before dereferencing them.
 */
class GenericArchive {
public:
    GenericArchive(void) {
        fPathName = nil;
        fNumEntries = 0;
        fEntryHead = fEntryTail = nil;
        fReloadFlag = true;
        //fEntryIndex = nil;
    }
    virtual ~GenericArchive(void) {
        //WMSG0("Deleting GenericArchive\n");
        DeleteEntries();
        delete fPathName;
    }

    virtual GenericEntry* GetEntries(void) const {
        return fEntryHead;
    }
    virtual long GetNumEntries(void) const {
        return fNumEntries;
    }
    //virtual GenericEntry* GetEntry(long num) {
    //  ASSERT(num >= 0 && num < fNumEntries);
    //  if (fEntryIndex == nil)
    //      CreateIndex();
    //  return fEntryIndex[num];
    //}

    typedef enum {
        kResultUnknown = 0,
        kResultSuccess,         // open succeeded
        kResultFailure,         // open failed
        kResultCancel,          // open was cancelled by user
        kResultFileArchive,     // found a file archive rather than disk image
    } OpenResult;

    // Open an archive and do fun things with the innards.
    virtual OpenResult Open(const char* filename, bool readOnly,
        CString* pErrMsg) = 0;
    // Create a new archive with the specified name.
    virtual CString New(const char* filename, const void* options) = 0;
    // Flush any unwritten data to disk
    virtual CString Flush(void) = 0;
    // Force a re-read from the underlying storage.
    virtual CString Reload(void) = 0;
    // Do we allow modification?
    virtual bool IsReadOnly(void) const = 0;
    // Does the underlying storage have un-flushed modifications?
    virtual bool IsModified(void) const = 0;

    virtual bool GetReloadFlag(void) { return fReloadFlag; }
    virtual void ClearReloadFlag(void) { fReloadFlag = false; }

    // One of these for every sub-class.  This is used to ensure that, should
    // we need to down-cast an object, we did it correctly (without needing
    // to include RTTI support).
    typedef enum {
        kArchiveUnknown = 0,
        kArchiveNuFX,
        kArchiveBNY,
        kArchiveACU,
        kArchiveDiskImage,
    } ArchiveKind;
    virtual ArchiveKind GetArchiveKind(void) = 0;

    // Get a nice description for the title bar.
    virtual void GetDescription(CString* pStr) const = 0;

    // Do a bulk add.
    virtual bool BulkAdd(ActionProgressDialog* pActionProgress,
        const AddFilesDialog* pAddOpts) = 0;
    // Do a disk add.
    virtual bool AddDisk(ActionProgressDialog* pActionProgress,
        const AddFilesDialog* pAddOpts) = 0;
    // Create a subdirectory.
    virtual bool CreateSubdir(CWnd* pMsgWnd, GenericEntry* pParentEntry,
        const char* newName) = 0;

    // Test a set of files.
    virtual bool TestSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) = 0;

    // Delete a set of files.
    virtual bool DeleteSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) = 0;

    // Rename a set of files.
    virtual bool RenameSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) = 0;
    virtual CString TestPathName(const GenericEntry* pGenericEntry,
        const CString& basePath, const CString& newName, char newFssep) const = 0;

    // Rename a volume (or sub-volume)
    virtual bool RenameVolume(CWnd* pMsgWnd, DiskFS* pDiskFS,
        const char* newName) = 0;
    virtual CString TestVolumeName(const DiskFS* pDiskFS,
        const char* newName) const = 0;

    // Recompress a set of files.
    virtual bool RecompressSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
        const RecompressOptionsDialog* pRecompOpts) = 0;

    // Transfer files out of this archive and into another.
    typedef enum {
        kXferOK = 0, kXferFailed = 1, kXferCancelled = 2, kXferOutOfSpace = 3
    } XferStatus;
    virtual XferStatus XferSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
        ActionProgressDialog* pActionProgress,
        const XferFileOptions* pXferOpts) = 0;

    // Get, set, or delete the comment on an entry.
    virtual bool GetComment(CWnd* pMsgWnd, const GenericEntry* pEntry,
        CString* pStr) = 0;
    virtual bool SetComment(CWnd* pMsgWnd, GenericEntry* pEntry,
        const CString& str) = 0;
    virtual bool DeleteComment(CWnd* pMsgWnd, GenericEntry* pEntry) = 0;

    // Set ProDOS file properties (e.g. file type, access flags).
    virtual bool SetProps(CWnd* pMsgWnd, GenericEntry* pEntry,
        const FileProps* pProps) = 0;

    // Preferences have changed, update library state as needed.
    virtual void PreferencesChanged(void) = 0;

    // Determine an archive's capabilities.  This is specific to the object
    // instance, so this must not be made a static function.
    typedef enum {
        kCapUnknown = 0,

        kCapCanTest,            // NuFX, BNY
        kCapCanRenameFullPath,  // NuFX, BNY
        kCapCanRecompress,      // NuFX, BNY
        kCapCanEditComment,     // NuFX
        kCapCanAddDisk,         // NuFX
        kCapCanConvEOLOnAdd,    // Disk
        kCapCanCreateSubdir,    // Disk
        kCapCanRenameVolume,    // Disk
    } Capability;
    virtual long GetCapability(Capability cap) = 0;

    // Get the pathname of the file we opened.
    const char* GetPathName(void) const { return fPathName; }

    // Generic utility function.
    static CString GenDerivedTempName(const char* filename);
    static int ComparePaths(const CString& name1, char fssep1,
        const CString& name2, char fssep2);

    void AddEntry(GenericEntry* pEntry);

    /*
     * This class holds details about a file that we're adding.
     *
     * It's based on the NuFileDetails class from NufxLib (which used to be
     * used everywhere).
     */
    class FileDetails {
    public:
        FileDetails(void);
        virtual ~FileDetails(void) {}

        /*
         * Automatic cast to NuFileDetails.  The NuFileDetails structure will
         * have a pointer to at least one of our strings, so structures
         * filled out this way need to be short-lived.  (Yes, this is
         * annoying, but it's how NufxLib works.)
         */
        operator const NuFileDetails() const;

        /*
         * Provide operator= and copy constructor.  This'd be easier without
         * the strings.
         */
        FileDetails& operator=(const FileDetails& src) {
            if (&src != this)
                CopyFields(this, &src);
            return *this;
        }
        FileDetails(const FileDetails& src) {
            CopyFields(this, &src);
        }

        /*
         * What kind of file this is.  Files being added to NuFX from Windows
         * can't be "BothForks" because each the forks are stored in
         * separate files.  However, files being transferred from a NuFX
         * archive, a disk image, or in from the clipboard can be both.
         *
         * (NOTE: this gets embedded into clipboard data.  If you change
         * these values, update the version info in Clipboard.cpp.)
         */
        typedef enum FileKind {
            kFileKindUnknown = 0,
            kFileKindDataFork,
            kFileKindRsrcFork,
            kFileKindDiskImage,
            kFileKindBothForks,
            kFileKindDirectory,
        } FileKind;

        /*
         * Data fields.  While transitioning from general use of NuFileDetails
         * (v1.2.x to v2.0) I'm just going to leave these public.
         */
        //NuThreadID          threadID;       /* data, rsrc, disk img? */
        FileKind            entryKind;
        CString             origName;

        CString             storageName;    /* normalized (NOT FS-normalized) */
        //NuFileSysID         fileSysID;
        DiskImg::FSFormat   fileSysFmt;
        unsigned short      fileSysInfo;    /* fssep lurks here */
        unsigned long       access;
        unsigned long       fileType;
        unsigned long       extraType;
        unsigned short      storageType;    /* "Unknown" or disk block size */
        NuDateTime          createWhen;
        NuDateTime          modWhen;
        NuDateTime          archiveWhen;

    private:
        static void CopyFields(FileDetails* pDst, const FileDetails* pSrc);
    };

    // Transfer files, one at a time, into this archive from another.
    virtual void XferPrepare(const XferFileOptions* pXferOpts) = 0;
    virtual CString XferFile(FileDetails* pDetails, unsigned char** pDataBuf,
        long dataLen, unsigned char** pRsrcBuf, long rsrcLen) = 0;
    virtual void XferAbort(CWnd* pMsgWnd) = 0;
    virtual void XferFinish(CWnd* pMsgWnd) = 0;
    static void UNIXTimeToDateTime(const time_t* pWhen, NuDateTime *pDateTime);

protected:
    virtual void DeleteEntries(void);

    /* NuLib2-derived recursive directory add functions */
    void ReplaceFssep(char* str, char oldc, char newc, char newSubst);
    NuError GetFileDetails(const AddFilesDialog* pAddOpts, const char* pathname,
        struct stat* psb, FileDetails* pDetails);
    Win32dirent* OpenDir(const char* name);
    Win32dirent* ReadDir(Win32dirent* dir);
    void CloseDir(Win32dirent* dir);
    NuError Win32AddDirectory(const AddFilesDialog* pAddOpts,
        const char* dirName, CString* pErrMsg);
    NuError Win32AddFile(const AddFilesDialog* pAddOpts,
        const char* pathname, CString* pErrMsg);
    NuError AddFile(const AddFilesDialog* pAddOpts, const char* pathname,
        CString* pErrMsg);

    /*
     * Each implementation must provide this.  It's called from the generic
     * AddFile function with the high-level add options, a partial pathname,
     * and a FileDetails structure filled in using Win32 calls.
     *
     * One call to AddFile can result in multiple calls to DoAddFile if
     * the subject of the AddFile call is a directory (and fIncludeSubdirs
     * is set).
     *
     * DoAddFile is not called for subdirectories.  The underlying code must
     * create directories as needed.
     *
     * In some cases (such as renaming a file as it is being added) the
     * information in "*pDetails" may be modified.
     */
    virtual NuError DoAddFile(const AddFilesDialog* pAddOpts,
        FileDetails* pDetails) = 0;

    void SetPathName(const char* pathName) {
        delete fPathName;
        if (pathName != nil) {
            fPathName = new char[strlen(pathName)+1];
            strcpy(fPathName, pathName);
        } else
            fPathName = nil;
    }

    bool            fReloadFlag;        // set after Reload called

private:
    //virtual void CreateIndex(void);

    //CString       fNewPathHolder;
    //CString       fOrigPathHolder;

    char*           fPathName;
    long            fNumEntries;
    GenericEntry*   fEntryHead;
    GenericEntry*   fEntryTail;
    //GenericEntry**    fEntryIndex;
};

/*
 * One entry in a SelectionSet.
 */
class SelectionEntry {
public:
    SelectionEntry(GenericEntry* pEntry) {
        fpEntry = pEntry;
        //fThreadKind = threadKind;
        //fFilter = filter;
        //fReformatName = "";
        fpPrev = fpNext = nil;
    }
    ~SelectionEntry(void) {}

    int Reformat(ReformatHolder* pHolder);

    GenericEntry* GetEntry(void) const { return fpEntry; }
    //int GetThreadKind(void) const { return fThreadKind; }
    //int GetFilter(void) const { return fFilter; }
    //const char* GetReformatName(void) const { return fReformatName; }

    SelectionEntry* GetPrev(void) const { return fpPrev; }
    void SetPrev(SelectionEntry* pPrev) { fpPrev = pPrev; }
    SelectionEntry* GetNext(void) const { return fpNext; }
    void SetNext(SelectionEntry* pNext) { fpNext = pNext; }

private:
    GenericEntry*   fpEntry;
    //int               fThreadKind;    // data, rsrc, etc (threadMask)
    //int               fFilter;        // fAllowedFilters, really
    //const char*       fReformatName;  // name of formatting actually applied

    SelectionEntry* fpPrev;
    SelectionEntry* fpNext;
};

class ContentList;

/*
 * A set of selected files.
 *
 * Each entry represents one item that can be displayed, such as a data
 * fork, resource fork, or comment thread.  Thus, a single file may have
 * multiple entries in the set.
 */
class SelectionSet {
public:
    SelectionSet(void) {
        fNumEntries = 0;
        fEntryHead = fEntryTail = fIterCurrent = nil;
    }
    ~SelectionSet(void) {
        DeleteEntries();
    }

    // create the set from the selected members of a ContentList
    void CreateFromSelection(ContentList* pContentList, int threadMask);
    // create the set from all members of a ContentList
    void CreateFromAll(ContentList* pContentList, int threadMask);

    // get the head of the list
    SelectionEntry* GetEntries(void) const { return fEntryHead; }

    void IterReset(void) {
        fIterCurrent = nil;
    }
    // move to the next or previous entry as part of iterating
    SelectionEntry* IterPrev(void) {
        if (fIterCurrent == nil)
            fIterCurrent = fEntryTail;
        else
            fIterCurrent = fIterCurrent->GetPrev();
        return fIterCurrent;
    }
    SelectionEntry* IterNext(void) {
        if (fIterCurrent == nil)
            fIterCurrent = fEntryHead;
        else
            fIterCurrent = fIterCurrent->GetNext();
        return fIterCurrent;
    }
    SelectionEntry* IterCurrent(void) {
        return fIterCurrent;
    }
    bool IterHasPrev(void) const {
        if (fIterCurrent == nil)
            return fEntryTail != nil;
        else
            return (fIterCurrent->GetPrev() != nil);
    }
    bool IterHasNext(void) const {
        if (fIterCurrent == nil)
            return fEntryHead != nil;
        else
            return (fIterCurrent->GetNext() != nil);
    }

    int GetNumEntries(void) const { return fNumEntries; }

    // count the #of entries whose display name matches "prefix"
    int CountMatchingPrefix(const char* prefix);

    // debug dump
    void Dump(void);

private:
    void AddToSet(GenericEntry* pEntry, int threadMask);

    void AddEntry(SelectionEntry* pEntry);
    void DeleteEntries(void);

    int             fNumEntries;
    SelectionEntry* fIterCurrent;

    SelectionEntry* fEntryHead;
    SelectionEntry* fEntryTail;
};

#endif /*__GENERIC_ARCHIVE__*/