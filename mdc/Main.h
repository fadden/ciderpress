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
    MainWindow(void);
    ~MainWindow(void);

private:
    afx_msg void OnFileScan(void);
    afx_msg void OnFileExit(void);
    afx_msg void OnHelpWebSite(void);
    afx_msg void OnHelpAbout(void);

    BOOL PeekAndPump(void);

    static void DebugMsgHandler(const char* file, int line,
        const char* msg);
    static NuResult NufxErrorMsgHandler(NuArchive* /*pArchive*/,
        void* vErrorMessage);

    void ScanFiles(void);
    Win32dirent* OpenDir(const WCHAR* name);
    Win32dirent* ReadDir(Win32dirent* dir);
    void CloseDir(Win32dirent* dir);
    int Process(const WCHAR* pathname, ScanOpts* pScanOpts,
        CString* pErrMsg);
    int ProcessDirectory(const WCHAR* dirName, ScanOpts* pScanOpts,
        CString* pErrMsg);
    int ScanDiskImage(const WCHAR* pathName, ScanOpts* pScanOpts);
    int LoadDiskFSContents(DiskFS* pDiskFS, const char* volName,
        ScanOpts* pScanOpts);
    void AnalyzeFile(const A2File* pFile, RecordKind* pRecordKind,
        LONGLONG* pTotalLen, LONGLONG* pTotalCompLen);
    bool IsRecordReadOnly(int access);
    static const char* GetFileTypeString(unsigned long fileType);

    bool    fCancelFlag;
    //int       fTitleAnimation;

    DECLARE_MESSAGE_MAP()
};

#endif /*MDC_MAIN_H*/
