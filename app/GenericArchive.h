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
struct FileProps {
    uint32_t    fileType;
    uint32_t    auxType;
    uint32_t    access;
    time_t      createWhen;
    time_t      modWhen;
};

/*
 * Options for converting between file archives and disk archives.
 */
class XferFileOptions {
public:
    XferFileOptions() :
        fTarget(NULL), fPreserveEmptyFolders(false), fpTargetFS(NULL)
        {}
    ~XferFileOptions() {}

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
    GenericEntry();
    virtual ~GenericEntry();

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
    enum ConvertEOL {
        kConvertUnknown = 0, kConvertEOLOff, kConvertEOLOn, kConvertEOLAuto
    };
    enum EOLType {
        kEOLUnknown = 0, kEOLCR, kEOLLF, kEOLCRLF
    };
    /* high ASCII conversion mode for threads being extracted */
    enum ConvertHighASCII {
        kConvertHAUnknown = 0, kConvertHAOff, kConvertHAOn, kConvertHAAuto
    };

    /* ProDOS access flags, used for all filesystems */
    enum {
        kAccessRead         = 0x01,
        kAccessWrite        = 0x02,
        kAccessInvisible    = 0x04,
        kAccessBackup       = 0x20,
        kAccessRename       = 0x40,
        kAccessDelete       = 0x80
    };

    /*
     * Features supported by underlying archive.  Primarily of interest
     * to EditPropsDialog, which needs to know what sort of attributes can
     * be altered in the file type and access flags.
     */
    enum Feature {
        kFeatureCanChangeType,
        kFeaturePascalTypes,
        kFeatureDOSTypes,
        kFeatureHFSTypes,
        kFeatureHasFullAccess,
        kFeatureHasSimpleAccess,    // mutually exclusive with FullAccess
        kFeatureHasInvisibleFlag,
    };

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
    virtual long GetSelectionSerial() const = 0;

    /* what operations are possible with this entry? */
    virtual bool GetFeatureFlag(Feature feature) const = 0;

    long GetIndex() const { return fIndex; }
    void SetIndex(long idx) { fIndex = idx; }

    /*
     * Set the pathname.  This comes from a file archive or disk image,
     * so it's always in Mac OS Roman format.
     *
     * Calling this will invalidate any strings previously returned by
     * GetPathName*(), GetFileName*(), and GetDisplayName().
     */
    void SetPathNameMOR(const char* pathNameMOR);

    const CStringA& GetPathNameMOR() const { return fPathNameMOR; }
    const CString& GetPathNameUNI() const { return fPathNameUNI; }
    const CString& GetFileName();
    const CString& GetFileNameExtension(); // returns e.g. ".SHK"
    const CStringA& GetFileNameExtensionMOR();
    /*
     * Returns the "display" name.  This is a combination of the sub-volume
     * name and the path name.  This string is intended for display only,
     * and may include characters that aren't legal on the filesystem.
     */
    const CString& GetDisplayName() const;

    void SetSubVolName(const WCHAR* name);
    const CString& GetSubVolName() const { return fSubVolName; }

    char GetFssep() const { return fFssep; }
    void SetFssep(char fssep) { fFssep = fssep; }
    uint32_t GetFileType() const { return fFileType; }
    void SetFileType(uint32_t type) { fFileType = type; }
    uint32_t GetAuxType() const { return fAuxType; }
    void SetAuxType(uint32_t type) { fAuxType = type; }
    uint32_t GetAccess() const { return fAccess; }
    void SetAccess(uint32_t access) { fAccess = access; }
    time_t GetCreateWhen() const { return fCreateWhen; }
    void SetCreateWhen(time_t when) { fCreateWhen = when; }
    time_t GetModWhen() const { return fModWhen; }
    void SetModWhen(time_t when) { fModWhen = when; }
    RecordKind GetRecordKind() const { return fRecordKind; }
    void SetRecordKind(RecordKind recordKind) { fRecordKind = recordKind; }
    const WCHAR* GetFormatStr() const { return fFormatStr; }
    void SetFormatStr(const WCHAR* str) { fFormatStr = str; } // arg not copied, must be static!
    LONGLONG GetCompressedLen() const { return fCompressedLen; }
    void SetCompressedLen(LONGLONG len) { fCompressedLen = len; }
    LONGLONG GetUncompressedLen() const {
        return fDataForkLen + fRsrcForkLen;
    }
    LONGLONG GetDataForkLen() const { return fDataForkLen; }
    void SetDataForkLen(LONGLONG len) { fDataForkLen = len; }
    LONGLONG GetRsrcForkLen() const { return fRsrcForkLen; }
    void SetRsrcForkLen(LONGLONG len) { fRsrcForkLen = len; }

    DiskImg::FSFormat GetSourceFS() const { return fSourceFS; }
    void SetSourceFS(DiskImg::FSFormat fmt) { fSourceFS = fmt; }

    bool GetHasDataFork() const { return fHasDataFork; }
    void SetHasDataFork(bool val) { fHasDataFork = val; }
    bool GetHasRsrcFork() const { return fHasRsrcFork; }
    void SetHasRsrcFork(bool val) { fHasRsrcFork = val; }
    bool GetHasDiskImage() const { return fHasDiskImage; }
    void SetHasDiskImage(bool val) { fHasDiskImage = val; }
    bool GetHasComment() const { return fHasComment; }
    void SetHasComment(bool val) { fHasComment = val; }
    bool GetHasNonEmptyComment() const { return fHasNonEmptyComment; }
    void SetHasNonEmptyComment(bool val) { fHasNonEmptyComment = val; }

    bool GetDamaged() const { return fDamaged; }
    void SetDamaged(bool val) { fDamaged = val; }
    bool GetSuspicious() const { return fSuspicious; }
    void SetSuspicious(bool val) { fSuspicious = val; }

    GenericEntry* GetPrev() const { return fpPrev; }
    void SetPrev(GenericEntry* pEntry) { fpPrev = pEntry; }
    GenericEntry* GetNext() const { return fpNext; }
    void SetNext(GenericEntry* pEntry) { fpNext = pEntry; }

    /*
     * Get a string for this entry's filetype.
     */
    const WCHAR* GetFileTypeString() const;

    /*
     * Check to see if this is a high-ASCII file.
     */
    static bool CheckHighASCII(const uint8_t* buffer, size_t count);

    /*
     * Decide, based on the contents of the buffer, whether we should do an
     * EOL conversion on the data.
     *
     * Returns kConvEOLOff or kConvEOLOn.
     */
    static ConvertEOL DetermineConversion(const uint8_t* buffer,
        size_t count, EOLType* pSourceType, ConvertHighASCII* pConvHA);

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

private:
    /*
     * Convert spaces to underscores, modifying the string.
     */
    static void SpacesToUnderscores(CStringA* pStr);

private:
    /*
     * This represents a file from an archive or disk image, so the Mac OS
     * Roman representation is the "true" version.  The Unicode version
     * is how we will use it on Windows (e.g. for file extraction), so
     * it will be a CP-1252 conversion until the various libraries
     * support UTF-16 filenames.
     *
     * The "display name" is only used for display, and should do a proper
     * MOR to Unicode conversion so the file name looks right.
     */

    CStringA    fPathNameMOR;       // original path name, Mac OS Roman chars
    CString     fPathNameUNI;       // Unicode conversion
    CString     fFileName;          // filename component of fPathNameUNI
    CString     fFileNameExtension; // filename extension from fPathNameUNI
    CStringA    fFileNameExtensionMOR;
    char        fFssep;
    CString     fSubVolName;        // sub-volume prefix, or NULL if none
    mutable CString fDisplayName;   // combination of sub-vol and path
    uint32_t    fFileType;
    uint32_t    fAuxType;
    uint32_t    fAccess;
    time_t      fCreateWhen;
    time_t      fModWhen;
    RecordKind  fRecordKind;        // forked file, disk image, ??
    const WCHAR* fFormatStr;        // static str; compression or fs format
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
    GenericArchive() {
        fPathName = NULL;
        fNumEntries = 0;
        fEntryHead = fEntryTail = NULL;
        fReloadFlag = true;
        //fEntryIndex = NULL;
    }
    virtual ~GenericArchive() {
        //LOGI("Deleting GenericArchive");
        DeleteEntries();
        delete fPathName;
    }

    virtual GenericEntry* GetEntries() const {
        return fEntryHead;
    }
    virtual long GetNumEntries() const {
        return fNumEntries;
    }

    enum OpenResult {
        kResultUnknown = 0,
        kResultSuccess,         // open succeeded
        kResultFailure,         // open failed
        kResultCancel,          // open was cancelled by user
        kResultFileArchive,     // found a file archive rather than disk image
    };

    // Open an archive and do fun things with the innards.
    virtual OpenResult Open(const WCHAR* filename, bool readOnly,
        CString* pErrMsg) = 0;
    // Create a new archive with the specified name.
    virtual CString New(const WCHAR* filename, const void* options) = 0;
    // Flush any unwritten data to disk
    virtual CString Flush() = 0;
    // Force a re-read from the underlying storage.
    virtual CString Reload() = 0;
    // Do we allow modification?
    virtual bool IsReadOnly() const = 0;
    // Does the underlying storage have un-flushed modifications?
    virtual bool IsModified() const = 0;

    virtual bool GetReloadFlag() { return fReloadFlag; }
    virtual void ClearReloadFlag() { fReloadFlag = false; }

    // One of these for every sub-class.
    enum ArchiveKind {
        kArchiveUnknown = 0,
        kArchiveNuFX,
        kArchiveBNY,
        kArchiveACU,
        kArchiveAppleSingle,
        kArchiveDiskImage,
    };

    // Returns the kind of archive this is (disk image, NuFX, BNY, etc).
    virtual ArchiveKind GetArchiveKind() = 0;

    // Get a nice description for the title bar.
    virtual CString GetDescription() const = 0;

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
    enum XferStatus {
        kXferOK = 0, kXferFailed = 1, kXferCancelled = 2, kXferOutOfSpace = 3
    };

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
    virtual void PreferencesChanged() = 0;

    // Determine an archive's capabilities.  This is specific to the object
    // instance, so this must not be made a static function.
    enum Capability {
        kCapUnknown = 0,

        kCapCanTest,            // NuFX, BNY
        kCapCanRenameFullPath,  // NuFX, BNY
        kCapCanRecompress,      // NuFX, BNY
        kCapCanEditComment,     // NuFX
        kCapCanAddDisk,         // NuFX
        kCapCanConvEOLOnAdd,    // Disk
        kCapCanCreateSubdir,    // Disk
        kCapCanRenameVolume,    // Disk
    };
    virtual long GetCapability(Capability cap) = 0;

    // Get the pathname of the file we opened.
    const WCHAR* GetPathName() const { return fPathName; }

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
     * This class holds details about a file that we're adding from local disk.
     */
    class LocalFileDetails {
    public:
        LocalFileDetails();
        virtual ~LocalFileDetails() {}

        /*
         * Set the various fields, based on the pathname and characteristics
         * of the file.
         */
        NuError SetFields(const AddFilesDialog* pAddOpts, const WCHAR* pathname,
            struct _stat* psb);

        /*
         * What kind of file this is.  Files being added to NuFX from Windows
         * can't be "BothForks" because the forks are stored in
         * separate files.  However, files being transferred from a NuFX
         * archive, a disk image, or in from the clipboard can be both.
         *
         * (NOTE: this gets embedded into clipboard data.  If you change
         * these values, update the version number in Clipboard.cpp.)
         */
        enum FileKind {
            kFileKindUnknown = 0,
            kFileKindDataFork,
            kFileKindRsrcFork,
            kFileKindDiskImage,
            kFileKindBothForks,
            kFileKindDirectory,
        };

        /*
         * Provide operator= and copy constructor.
         */
        LocalFileDetails& operator=(const LocalFileDetails& src) {
            if (&src != this)
                CopyFields(this, &src);
            return *this;
        }
        LocalFileDetails(const LocalFileDetails& src) {
            CopyFields(this, &src);
        }

        /*
         * Returns a reference to a NuFileDetails structure with the contents
         * of the LocalFileDetails.
         *
         * The returned structure may not be used after the LocalFileDetails
         * is modified or released.
         */
        const NuFileDetails& GetNuFileDetails();

        /*
         * Returns a reference to a NuFileDetails structure with the contents
         * of the LocalFileDetails.
         *
         * The returned structure may not be used after the LocalFileDetails
         * is modified or released.
         */
        const DiskFS::CreateParms& GetCreateParms();

        /*
         * Returns the "local" pathname, i.e. the name of a Windows file.
         */
        const CString& GetLocalPathName() const {
            return fLocalPathName;
        }

        void SetLocalPathName(const CString& newName) {
            fLocalPathName = newName;
        }

        /*
         * This is the "local" pathname with any file type preservation
         * strings removed.
         */
        const CString& GetStrippedLocalPathName() const {
            return fStrippedLocalPathName;
        }

        /*
         * Sets the "stripped" local path name, and updates the MOR version.
         * Does not alter the full local path name.
         */
        void SetStrippedLocalPathName(const CString& newName) {
            fStrippedLocalPathName = newName;
            GenerateStoragePathName();
        }

        /*
         * Returns a copy of the stripped local pathname that has been
         * converted to Mac OS Roman.
         *
         * The returned string will be invalid if SetStrippedLocalPathName
         * is subsequently called.
         */
        const CStringA& GetStoragePathNameMOR() const {
            return fStoragePathNameMOR;
        }

        FileKind GetEntryKind() const { return fEntryKind; }
        void SetEntryKind(FileKind kind) { fEntryKind = kind;  }

        DiskImg::FSFormat GetFileSysFmt() const { return fFileSysFmt; }
        void SetFileSysFmt(DiskImg::FSFormat fmt) { fFileSysFmt = fmt; }

        char GetFssep() const { return fFssep; }
        void SetFssep(char fssep) { fFssep = fssep;  }

        uint32_t GetFileType() const { return fFileType; }
        void SetFileType(uint32_t type) { fFileType = type; }

        uint32_t GetExtraType() const { return fExtraType; }
        void SetExtraType(uint32_t type) { fExtraType = type; }

        uint32_t GetAccess() const { return fAccess; }
        void SetAccess(uint32_t access) { fAccess = access; }

        uint16_t GetStorageType() const { return fStorageType; }
        void SetStorageType(uint16_t type) { fStorageType = type; }

        const NuDateTime& GetArchiveWhen() const { return fArchiveWhen; }
        void SetArchiveWhen(const NuDateTime& when) { fArchiveWhen = when; }

        const NuDateTime& GetModWhen() const { return fModWhen;  }
        void SetModWhen(const NuDateTime& when) { fModWhen = when; }

        const NuDateTime& GetCreateWhen() const { return fCreateWhen; }
        void SetCreateWhen(const NuDateTime& when) { fCreateWhen = when; }

    private:
        /*
         * Copy the contents of our object to a new object, field by field,
         * so the CStrings copy correctly.
         *
         * Useful for operator= and copy construction.
         */
        static void CopyFields(LocalFileDetails* pDst,
            const LocalFileDetails* pSrc);

        /*
         * Generates fStoragePathNameMOR from fStrippedLocalPathName.
         */
        void GenerateStoragePathName();

        /*
         * These are the structs that the libraries want, so we provide calls
         * that populate them and return a reference.  These are "hairy"
         * structures, so we have to place limitations on their lifetime.
         *
         * Ideally these would either be proper objects with destructors,
         * so we could create them and not worry about who will free up the
         * hairy bits, or we would omit the hairy bits from the structure and
         * pass them separately.
         */
        NuFileDetails       fNuFileDetails;
        DiskFS::CreateParms fCreateParms;

        /*
         * What does this entry represent?
         */
        FileKind            fEntryKind;

        /*
         * Full pathname of the Windows file.
         */
        CString             fLocalPathName;         // was origName

        /*
         * "Stripped" pathname.  This is the full path with any of our
         * added bits removed (e.g. file type & fork identifiers).
         *
         * This is generated by PathProposal::LocalToArchive().
         */
        CString             fStrippedLocalPathName; // was storageName

        /*
         * The storage name, generated by converting strippedLocalPathName
         * from Unicode to Mac OS Roman.  This is what will be passed to
         * DiskImg and NufxLib as the name of the file to create in the
         * archive.  For DiskImg there's an additional "normalization" pass
         * make the filename suitable for use on the filesystem; that name
         * is stored in a FileAddData object, not here.
         */
        CStringA            fStoragePathNameMOR;

        DiskImg::FSFormat   fFileSysFmt;    // only set for xfers?
        uint8_t             fFssep;
        uint32_t            fFileType;
        uint32_t            fExtraType;
        uint32_t            fAccess;
        uint16_t            fStorageType;   // "Unknown" or disk block size
        NuDateTime          fCreateWhen;
        NuDateTime          fModWhen;
        NuDateTime          fArchiveWhen;
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
    virtual CString XferFile(LocalFileDetails* pDetails, uint8_t** pDataBuf,
        long dataLen, uint8_t** pRsrcBuf, long rsrcLen) = 0;

    // Abort progress.  Not all subclasses are capable of "undo".
    virtual void XferAbort(CWnd* pMsgWnd) = 0;

    // Transfer is finished.
    virtual void XferFinish(CWnd* pMsgWnd) = 0;

    /*
     * Convert from time in seconds to Apple IIgs DateTime format.
     */
    static void UNIXTimeToDateTime(const time_t* pWhen, NuDateTime *pDateTime);

    /*
     * Reads a 16-bit big-endian value from a buffer.  Does not guard
     * against buffer overrun.
     */
    static uint16_t Get16BE(const uint8_t* ptr) {
        return ptr[1] | (ptr[0] << 8);
    }

    /*
     * Reads a 32-bit big-endian value from a buffer.  Does not guard
     * against buffer overrun.
     */
    static uint32_t Get32BE(const uint8_t* ptr) {
        return ptr[3] | (ptr[2] << 8) | (ptr[1] << 16) | (ptr[0] << 24);
    }

    /*
     * Reads a 16-bit little-endian value from a buffer.  Does not guard
     * against buffer overrun.
     */
    static uint16_t Get16LE(const uint8_t* ptr) {
        return ptr[0] | (ptr[1] << 8);
    }

    /*
     * Reads a 32-bit little-endian value from a buffer.  Does not guard
     * against buffer overrun.
     */
    static uint32_t Get32LE(const uint8_t* ptr) {
        return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    }

protected:
    /*
     * Delete the "entries" list.
     */
    virtual void DeleteEntries();

    void ReplaceFssep(WCHAR* str, char oldc, char newc, char newSubst);

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
        LocalFileDetails* pDetails) = 0;

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
    //virtual void CreateIndex();

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
    ~SelectionEntry() {}

    int Reformat(ReformatHolder* pHolder);

    GenericEntry* GetEntry() const { return fpEntry; }
    //int GetThreadKind() const { return fThreadKind; }
    //int GetFilter() const { return fFilter; }
    //const char* GetReformatName() const { return fReformatName; }

    SelectionEntry* GetPrev() const { return fpPrev; }
    void SetPrev(SelectionEntry* pPrev) { fpPrev = pPrev; }
    SelectionEntry* GetNext() const { return fpNext; }
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
    SelectionSet() {
        fNumEntries = 0;
        fEntryHead = fEntryTail = fIterCurrent = NULL;
    }
    ~SelectionSet() {
        DeleteEntries();
    }

    // create the set from the selected members of a ContentList
    void CreateFromSelection(ContentList* pContentList, int threadMask);
    // create the set from all members of a ContentList
    void CreateFromAll(ContentList* pContentList, int threadMask);

    // get the head of the list
    SelectionEntry* GetEntries() const { return fEntryHead; }

    void IterReset() {
        fIterCurrent = NULL;
    }
    // move to the next or previous entry as part of iterating
    SelectionEntry* IterPrev() {
        if (fIterCurrent == NULL)
            fIterCurrent = fEntryTail;
        else
            fIterCurrent = fIterCurrent->GetPrev();
        return fIterCurrent;
    }
    SelectionEntry* IterNext() {
        if (fIterCurrent == NULL)
            fIterCurrent = fEntryHead;
        else
            fIterCurrent = fIterCurrent->GetNext();
        return fIterCurrent;
    }
    SelectionEntry* IterCurrent() {
        return fIterCurrent;
    }
    bool IterHasPrev() const {
        if (fIterCurrent == NULL)
            return fEntryTail != NULL;
        else
            return (fIterCurrent->GetPrev() != NULL);
    }
    bool IterHasNext() const {
        if (fIterCurrent == NULL)
            return fEntryHead != NULL;
        else
            return (fIterCurrent->GetNext() != NULL);
    }

    int GetNumEntries() const { return fNumEntries; }

    // count the #of entries whose display name matches "prefix"
    int CountMatchingPrefix(const WCHAR* prefix);

    // debug dump the contents of the selection set
    void Dump();

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
    void DeleteEntries();

    int             fNumEntries;
    SelectionEntry* fIterCurrent;

    SelectionEntry* fEntryHead;
    SelectionEntry* fEntryTail;
};

#endif /*APP_GENERICARCHIVE_H*/
