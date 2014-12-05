/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Generic Apple II archive handling.  In the beginning we only handled
 * NuFX archives, and the code continues to reflect that heritage.
 *
 * These are abstract base classes.
 */
#ifndef APP_GENERICARCHIVE_H
#define APP_GENERICARCHIVE_H

#include "Preferences.h"
#include "../util/UtilLib.h"
#include "../diskimg/DiskImg.h"
#include "../nufxlib/NufxLib.h"
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
    uint32_t    fileType;
    uint32_t    auxType;
    uint32_t    access;
    time_t      createWhen;
    time_t      modWhen;
} FileProps;

/*
 * Options for converting between file archives and disk archives.
 */
class XferFileOptions {
public:
    XferFileOptions(void) :
        fTarget(NULL), fPreserveEmptyFolders(false), fpTargetFS(NULL)
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

    /*
     * Extract data from an archive (NuFX, disk image, etc).
     *
     * If "*ppText" is non-NULL, the data will be read into the pointed-to buffer
     * so long as it's shorter than *pLength bytes.  The value in "*pLength"
     * will be set to the actual length used.
     *
     * If "*ppText" is NULL, the uncompressed data will be placed into a buffer
     * allocated with "new[]".
     *
     * Returns IDOK on success, IDCANCEL if the operation was cancelled by the
     * user, and -1 value on failure.  On failure, "*pErrMsg" holds an error
     * message.
     *
     * "which" is an anonymous GenericArchive enum (e.g. "kDataThread").
     */
    virtual int ExtractThreadToBuffer(int which, char** ppText, long* pLength,
        CString* pErrMsg) const = 0;

    /*
     * Extract data from a thread or disk file to a Windows file.  Since we're
     * not copying to a buffer we can't assume that we're able to hold the
     * entire file in memory all at once.
     *
     * Returns IDOK on success, IDCANCEL if the operation was cancelled by the
     * user, and -1 value on failure.  On failure, "*pMsg" holds an
     * error message.
     */
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

    const WCHAR* GetPathName(void) const { return fPathName; }
    void SetPathName(const WCHAR* path);
    const WCHAR* GetFileName(void);
    const WCHAR* GetFileNameExtension(void); // returns e.g. ".SHK"
    CStringA GetFileNameExtensionA(void);
    void SetSubVolName(const WCHAR* name);
    const WCHAR* GetSubVolName(void) const { return fSubVolName; }
    const WCHAR* GetDisplayName(void) const; // not really "const"

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
    const WCHAR* GetFormatStr(void) const { return fFormatStr; }
    void SetFormatStr(const WCHAR* str) { fFormatStr = str; } // arg not copied, must be static!
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

    /*
     * Get a string for this entry's filetype.
     */
    const WCHAR* GetFileTypeString(void) const;

    /*
     * Check to see if this is a high-ASCII file.
     */
    static bool CheckHighASCII(const uint8_t* buffer,
        unsigned long count);

    /*
     * Decide, based on the contents of the buffer, whether we should do an
     * EOL conversion on the data.
     *
     * Returns kConvEOLOff or kConvEOLOn.
     */
    static ConvertEOL DetermineConversion(const uint8_t* buffer,
        long count, EOLType* pSourceType, ConvertHighASCII* pConvHA);

    /*
     * Write data to a file, possibly converting EOL markers to Windows CRLF
     * and stripping high ASCII.
     *
     * If "*pConv" is kConvertEOLAuto, this will try to auto-detect whether
     * the input is a text file or not by scanning the input buffer.
     *
     * Ditto for "*pConvHA".
     *
     * "fp" is the output file, "buf" is the input, "len" is the buffer length.
     * "*pLastCR" should initially be "false", and carried across invocations.
     *
     * Returns 0 on success, or an errno value on error.
     */
    static int GenericEntry::WriteConvert(FILE* fp, const char* buf,
        size_t len, ConvertEOL* pConv, ConvertHighASCII* pConvHA,
        bool* pLastCR);

protected:
    /*
     * Convert spaces to underscores, modifying the string.
     */
    static void SpacesToUnderscores(WCHAR* buf);

private:
    WCHAR*       fPathName;
    const WCHAR* fFileName;         // points within fPathName
    const WCHAR* fFileNameExtension; // points within fPathName
    char        fFssep;
    WCHAR*       fSubVolName;       // sub-volume prefix, or NULL if none
    WCHAR*      fDisplayName;       // combination of sub-vol and path
    long        fFileType;
    long        fAuxType;
    long        fAccess;
    time_t      fCreateWhen;
    time_t      fModWhen;
    RecordKind  fRecordKind;        // forked file, disk image, ??
    const WCHAR* fFormatStr;        // static str; compression or fs format
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
        fPathName = NULL;
        fNumEntries = 0;
        fEntryHead = fEntryTail = NULL;
        fReloadFlag = true;
        //fEntryIndex = NULL;
    }
    virtual ~GenericArchive(void) {
        //LOGI("Deleting GenericArchive");
        DeleteEntries();
        delete fPathName;
    }

    virtual GenericEntry* GetEntries(void) const {
        return fEntryHead;
    }
    virtual long GetNumEntries(void) const {
        return fNumEntries;
    }

    typedef enum {
        kResultUnknown = 0,
        kResultSuccess,         // open succeeded
        kResultFailure,         // open failed
        kResultCancel,          // open was cancelled by user
        kResultFileArchive,     // found a file archive rather than disk image
    } OpenResult;

    // Open an archive and do fun things with the innards.
    virtual OpenResult Open(const WCHAR* filename, bool readOnly,
        CString* pErrMsg) = 0;
    // Create a new archive with the specified name.
    virtual CString New(const WCHAR* filename, const void* options) = 0;
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

    // One of these for every sub-class.
    typedef enum {
        kArchiveUnknown = 0,
        kArchiveNuFX,
        kArchiveBNY,
        kArchiveACU,
        kArchiveDiskImage,
    } ArchiveKind;

    // Returns the kind of archive this is (disk image, NuFX, BNY, etc).
    virtual ArchiveKind GetArchiveKind(void) = 0;

    // Get a nice description for the title bar.
    virtual void GetDescription(CString* pStr) const = 0;

    // Do a bulk add.
    virtual bool BulkAdd(ActionProgressDialog* pActionProgress,
        const AddFilesDialog* pAddOpts) = 0;

    // Add a single disk to the archive.
    virtual bool AddDisk(ActionProgressDialog* pActionProgress,
        const AddFilesDialog* pAddOpts) = 0;

    // Create a subdirectory with name newName in pParentEntry.
    virtual bool CreateSubdir(CWnd* pMsgWnd, GenericEntry* pParentEntry,
        const WCHAR* newName) = 0;

    // Test a set of files.
    virtual bool TestSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) = 0;

    // Delete a set of files.
    virtual bool DeleteSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) = 0;

    // Rename a set of files.
    virtual bool RenameSelection(CWnd* pMsgWnd, SelectionSet* pSelSet) = 0;

    // Verify that a name is suitable.  Called by RenameEntryDialog and
    // CreateSubdirDialog.
    //
    // Tests for context-specific syntax and checks for duplicates.
    //
    // Returns an empty string on success, or an error message on failure.
    virtual CString TestPathName(const GenericEntry* pGenericEntry,
        const CString& basePath, const CString& newName, char newFssep) const = 0;

    // Rename a volume (or sub-volume)
    virtual bool RenameVolume(CWnd* pMsgWnd, DiskFS* pDiskFS,
        const WCHAR* newName) = 0;
    virtual CString TestVolumeName(const DiskFS* pDiskFS,
        const WCHAR* newName) const = 0;

    // Recompress a set of files.
    virtual bool RecompressSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
        const RecompressOptionsDialog* pRecompOpts) = 0;

    // return result from XferSelection()
    typedef enum {
        kXferOK = 0, kXferFailed = 1, kXferCancelled = 2, kXferOutOfSpace = 3
    } XferStatus;

    // Transfer selected files out of this archive and into another.
    virtual XferStatus XferSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
        ActionProgressDialog* pActionProgress,
        const XferFileOptions* pXferOpts) = 0;

    // Extract a comment from the archive, converting line terminators to CRLF.
    virtual bool GetComment(CWnd* pMsgWnd, const GenericEntry* pEntry,
        CString* pStr) = 0;

    // Set a comment on an entry.
    virtual bool SetComment(CWnd* pMsgWnd, GenericEntry* pEntry,
        const CString& str) = 0;

    // Delete the comment from the entry.
    virtual bool DeleteComment(CWnd* pMsgWnd, GenericEntry* pEntry) = 0;

    // Set ProDOS file properties (file type, aux type, access flags).
    virtual bool SetProps(CWnd* pMsgWnd, GenericEntry* pEntry,
        const FileProps* pProps) = 0;

    // Preferences have changed, update library state as needed.  Also called
    // the first time though.
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
    const WCHAR* GetPathName(void) const { return fPathName; }

    /*
     * Generate a temp name from a file name.
     */
    static CString GenDerivedTempName(const WCHAR* filename);

    /*
     * Do a strcasecmp-like comparison, taking equivalent fssep chars into
     * account.
     *
     * The tricky part is with files like "foo:bar" ':' -- "foo:bar" '/'.  The
     * names appear to match, but the fssep chars are different, so they don't.
     * If we just return (char1 - char2), though, we'll be returning 0 because
     * the ASCII values match even if the character *meanings* don't.
     *
     * This assumes that the fssep char is not affected by tolower().
     *
     * [This may not sort correctly...haven't verified that I'm returning the
     * right thing for ascending ASCII sort.]
     */
    static int ComparePaths(const CString& name1, char fssep1,
        const CString& name2, char fssep2);

    /*
     * Add a new entry to the end of the list.
     */
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
         *
         * TODO: make this a "GenNuFileDetails" method that returns a
         *  NuFileDetails struct owned by this object.  It fills out the
         *  fields and references internal strings.  Because we own the
         *  object, we can control the lifespan of the NuFileDetails, and
         *  ensure it doesn't live longer than our strings.
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
         * TODO: make this not public
         */

        //NuThreadID          threadID;       /* data, rsrc, disk img? */
        FileKind            entryKind;

        /*
         * Original full pathname as found on Windows.
         */
        CString             origName;

        /*
         * "Normalized" pathname.  This is the full path with any of our
         * added bits removed (e.g. file type & fork identifiers).  It has
         * not been sanitized for any specific target filesystem.
         *
         * This is generated by PathProposal::LocalToArchive().
         */
        CString             storageName;

        //NuFileSysID         fileSysID;
        DiskImg::FSFormat   fileSysFmt;
        uint16_t            fileSysInfo;    /* fssep lurks here */
        uint32_t            access;
        uint32_t            fileType;
        uint32_t            extraType;
        uint16_t            storageType;    /* "Unknown" or disk block size */
        NuDateTime          createWhen;
        NuDateTime          modWhen;
        NuDateTime          archiveWhen;


        // temporary kluge to get things working
        mutable CStringA    fOrigNameA;
        mutable CStringA    fStorageNameA;

    private:
        /*
         * Copy the contents of our object to a new object.
         *
         * Useful for operator= and copy construction.
         */
        static void CopyFields(FileDetails* pDst, const FileDetails* pSrc);
    };

    // Prepare for file transfers.
    virtual void XferPrepare(const XferFileOptions* pXferOpts) = 0;

    // Transfer files, one at a time, into this archive from another.  Called
    // from XferSelection and clipboard "paste".
    //
    // "dataLen" and "rsrcLen" will be -1 if the corresponding fork doesn't exist.
    // Returns 0 on success, nonzero on failure.
    //
    // On success, *pDataBuf and *pRsrcBuf are freed and set to NULL.  (It's
    // necessary for the interface to work this way because the NufxArchive
    // version just tucks the pointers into NufxLib structures.)
    virtual CString XferFile(FileDetails* pDetails, uint8_t** pDataBuf,
        long dataLen, uint8_t** pRsrcBuf, long rsrcLen) = 0;

    // Abort progress.  Not all subclasses are capable of "undo".
    virtual void XferAbort(CWnd* pMsgWnd) = 0;

    // Transfer is finished.
    virtual void XferFinish(CWnd* pMsgWnd) = 0;

    /*
     * Convert from time in seconds to Apple IIgs DateTime format.
     */
    static void UNIXTimeToDateTime(const time_t* pWhen, NuDateTime *pDateTime);

protected:
    /*
     * Delete the "entries" list.
     */
    virtual void DeleteEntries(void);

    void ReplaceFssep(WCHAR* str, char oldc, char newc, char newSubst);

    /*
     * Set the contents of a NuFileDetails structure, based on the pathname
     * and characteristics of the file.
     *
     * For efficiency and simplicity, the pathname fields are set to CStrings in
     * the GenericArchive object instead of newly-allocated storage.
     */
    NuError GetFileDetails(const AddFilesDialog* pAddOpts, const WCHAR* pathname,
        struct _stat* psb, FileDetails* pDetails);

    /*
     * Prepare a directory for reading.
     *
     * Allocates a Win32dirent struct that must be freed by the caller.
     */
    Win32dirent* OpenDir(const WCHAR* name);

    /*
     * Get an entry from an open directory.
     *
     * Returns a NULL pointer after the last entry has been read.
     */
    Win32dirent* ReadDir(Win32dirent* dir);

    /*
     * Close a directory.
     */
    void CloseDir(Win32dirent* dir);

    /*
     * Win32 recursive directory descent.  Scan the contents of a directory.
     * If a subdirectory is found, follow it; otherwise, call Win32AddFile to
     * add the file.
     */
    NuError Win32AddDirectory(const AddFilesDialog* pAddOpts,
        const WCHAR* dirName, CString* pErrMsg);

    /*
     * Add a file to the list we're adding to the archive.  If it's a directory,
     * and the recursive descent feature is enabled, call Win32AddDirectory to
     * add the contents of the dir.
     *
     * Returns with an error if the file doesn't exist or isn't readable.
     */
    NuError Win32AddFile(const AddFilesDialog* pAddOpts,
        const WCHAR* pathname, CString* pErrMsg);

    /*
     * External entry point; just calls the system-specific version.
     */
    NuError AddFile(const AddFilesDialog* pAddOpts, const WCHAR* pathname,
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

    void SetPathName(const WCHAR* pathName) {
        free(fPathName);
        if (pathName != NULL) {
            fPathName = _wcsdup(pathName);
        } else {
            fPathName = NULL;
        }
    }

    bool            fReloadFlag;        // set after Reload called

private:
    //virtual void CreateIndex(void);

    //CString       fNewPathHolder;
    //CString       fOrigPathHolder;

    WCHAR*          fPathName;
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
        fpPrev = fpNext = NULL;
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
        fEntryHead = fEntryTail = fIterCurrent = NULL;
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
        fIterCurrent = NULL;
    }
    // move to the next or previous entry as part of iterating
    SelectionEntry* IterPrev(void) {
        if (fIterCurrent == NULL)
            fIterCurrent = fEntryTail;
        else
            fIterCurrent = fIterCurrent->GetPrev();
        return fIterCurrent;
    }
    SelectionEntry* IterNext(void) {
        if (fIterCurrent == NULL)
            fIterCurrent = fEntryHead;
        else
            fIterCurrent = fIterCurrent->GetNext();
        return fIterCurrent;
    }
    SelectionEntry* IterCurrent(void) {
        return fIterCurrent;
    }
    bool IterHasPrev(void) const {
        if (fIterCurrent == NULL)
            return fEntryTail != NULL;
        else
            return (fIterCurrent->GetPrev() != NULL);
    }
    bool IterHasNext(void) const {
        if (fIterCurrent == NULL)
            return fEntryHead != NULL;
        else
            return (fIterCurrent->GetNext() != NULL);
    }

    int GetNumEntries(void) const { return fNumEntries; }

    // count the #of entries whose display name matches "prefix"
    int CountMatchingPrefix(const WCHAR* prefix);

    // debug dump the contents of the selection set
    void Dump(void);

private:
    /*
     * Add a GenericEntry to the set, but only if we can find a thread that
     * matches the flags in "threadMask".
     */
    void AddToSet(GenericEntry* pEntry, int threadMask);

    /*
     * Add a new entry to the end of the list.
     */
    void AddEntry(SelectionEntry* pEntry);

    /*
     * Delete the "entries" list.
     */
    void DeleteEntries(void);

    int             fNumEntries;
    SelectionEntry* fIterCurrent;

    SelectionEntry* fEntryHead;
    SelectionEntry* fEntryTail;
};

#endif /*APP_GENERICARCHIVE_H*/
