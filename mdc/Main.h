/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Main frame window declarations.
 */
#ifndef MDC_MAIN_H
#define MDC_MAIN_H

#include "../diskimg/DiskImg.h"
#include "../nufxlib/NufxLib.h"
using namespace DiskImgLib;

struct Win32dirent;
struct ScanOpts;

typedef enum RecordKind {
    kRecordKindUnknown = 0,
    kRecordKindDisk,
    kRecordKindFile,
    kRecordKindForkedFile,
    kRecordKindDirectory,
    kRecordKindVolumeDir,
} RecordKind;


/*
 * The main UI window.
 */
class MainWindow : public CFrameWnd {
public:
    /*
     * MainWindow constructor.  Creates the main window and sets
     * its properties.
     */
    MainWindow(void);

    /*
     * MainWindow destructor.  Close the archive if one is open, but don't try
     * to shut down any controls in child windows.  By this point, Windows has
     * already snuffed them.
     */
    ~MainWindow(void);

private:
    afx_msg void OnFileScan(void);
    afx_msg void OnFileExit(void);
    afx_msg void OnHelpWebSite(void);
    afx_msg void OnHelpAbout(void);

    /*
     * Allow events to flow through the message queue whenever the
     * progress meter gets updated.  This will allow us to redraw with
     * reasonable frequency.
     *
     * Calling this can result in other code being called, such as Windows
     * message handlers, which can lead to reentrancy problems.  Make sure
     * you're adequately semaphored before calling here.
     *
     * Returns TRUE if all is well, FALSE if we're trying to quit.
     */
    BOOL PeekAndPump(void);

    /*
     * Handle a debug message from the DiskImg library.
     */
    static void DebugMsgHandler(const char* file, int line,
        const char* msg);
    static NuResult NufxErrorMsgHandler(NuArchive* /*pArchive*/,
        void* vErrorMessage);

    /*
     * Prompts the user to select the input set and output file, then starts
     * the scan.
     */
    void ScanFiles(void);

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
     * Process a file or directory.  These are expected to be names of files in
     * the current directory.
     *
     * Returns 0 on success, nonzero on error with a message in "*pErrMsg".
     */
    int Process(const WCHAR* pathname, ScanOpts* pScanOpts,
        CString* pErrMsg);

    /*
     * Win32 recursive directory descent.  Scan the contents of a directory.
     * If a subdirectory is found, follow it; otherwise, call Win32AddFile to
     * add the file.
     */
    int ProcessDirectory(const WCHAR* dirName, ScanOpts* pScanOpts,
        CString* pErrMsg);

    /*
     * Open a disk image and dump the contents.
     *
     * Returns 0 on success, nonzero on failure.
     */
    int ScanDiskImage(const WCHAR* pathName, ScanOpts* pScanOpts);

    /*
     * Analyze a file's characteristics.
     */
    void AnalyzeFile(const A2File* pFile, RecordKind* pRecordKind,
        LONGLONG* pTotalLen, LONGLONG* pTotalCompLen);

    /*
     * Determine whether the access bits on the record make it a read-only
     * file or not.
     *
     * Uses a simplified view of the access flags.
     */
    bool IsRecordReadOnly(int access);

    /*
     * Return a pointer to the three-letter representation of the file type name.
     *
     * Note to self: code down below tests first char for '?'.
     */
    static const char* GetFileTypeString(unsigned long fileType);

    /*
     * Load the contents of a DiskFS.
     *
     * Recursively handle sub-volumes.
     */
    int LoadDiskFSContents(DiskFS* pDiskFS, const char* volName,
        ScanOpts* pScanOpts);

    bool    fCancelFlag;
    //int       fTitleAnimation;

    DECLARE_MESSAGE_MAP()
};

#endif /*MDC_MAIN_H*/
