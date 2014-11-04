/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Main window management.
 */
#include "stdafx.h"
#include "Main.h"
#include "mdc.h"
#include "AboutDlg.h"
#include "ChooseFilesDlg.h"
#include "ProgressDlg.h"
#include "resource.h"
#include "../diskimg/DiskImg.h"
#include "../prebuilt/zlib.h"

const char* kWebSiteURL = "http://www.faddensoft.com/";


BEGIN_MESSAGE_MAP(MainWindow, CFrameWnd)
    ON_COMMAND(IDM_FILE_SCAN, OnFileScan)
    ON_COMMAND(IDM_FILE_EXIT, OnFileExit)
    ON_COMMAND(IDM_HELP_WEBSITE, OnHelpWebSite)
    ON_COMMAND(IDM_HELP_ABOUT, OnHelpAbout)
END_MESSAGE_MAP()


/*
 * MainWindow constructor.  Creates the main window and sets
 * its properties.
 */
MainWindow::MainWindow()
{
    static const char* kAppName = "MDC";

    CString wndClass = AfxRegisterWndClass(
        CS_DBLCLKS /*| CS_HREDRAW | CS_VREDRAW*/,
        gMyApp.LoadStandardCursor(IDC_ARROW),
        (HBRUSH) (COLOR_WINDOW + 1),
        gMyApp.LoadIcon(IDI_MDC) );

    Create(wndClass, kAppName, WS_OVERLAPPEDWINDOW /*| WS_CLIPCHILDREN*/,
        rectDefault, NULL, MAKEINTRESOURCE(IDC_MDC));

    LoadAccelTable(MAKEINTRESOURCE(IDC_MDC));

    // initialize some OLE garbage
    //AfxOleInit();

    // required by MFC if Rich Edit controls are used
    //AfxInitRichEdit();

    DiskImgLib::Global::SetDebugMsgHandler(DebugMsgHandler);
    DiskImgLib::Global::AppInit();

    NuSetGlobalErrorMessageHandler(NufxErrorMsgHandler);

    //fTitleAnimation = 0;
    fCancelFlag = false;
}

/*
 * MainWindow destructor.  Close the archive if one is open, but don't try
 * to shut down any controls in child windows.  By this point, Windows has
 * already snuffed them.
 */
MainWindow::~MainWindow()
{
//  int cc;
//  cc = ::WinHelp(m_hWnd, ::AfxGetApp()->m_pszHelpFilePath, HELP_QUIT, 0);
//  WMSG1("Turning off WinHelp returned %d\n", cc);

    DiskImgLib::Global::AppCleanup();
}

/*
 * Handle Exit item by sending a close request.
 */
void
MainWindow::OnFileExit(void)
{
    SendMessage(WM_CLOSE, 0, 0);
}

/*
 * Go to the faddenSoft web site.
 */
void
MainWindow::OnHelpWebSite(void)
{
    int err;

    err = (int) ::ShellExecute(m_hWnd, _T("open"), kWebSiteURL, NULL, NULL,
                    SW_SHOWNORMAL);
    if (err <= 32) {
        CString msg;
        msg.Format("Unable to launch web browser (err=%d).", err);
        ShowFailureMsg(this, msg, IDS_FAILED);
    }
}

/*
 * Pop up the About box.
 */
void
MainWindow::OnHelpAbout(void)
{
    int result;

    AboutDlg dlg(this);

    result = dlg.DoModal();
    WMSG1("HelpAbout returned %d\n", result);
}


/*
 * Handle "scan" item.
 */
void
MainWindow::OnFileScan(void)
{
    if (0) {
        CString msg;
        msg.LoadString(IDS_MUST_REGISTER);
        ShowFailureMsg(this, msg, IDS_APP_TITLE);
    } else {
        ScanFiles();
    }
}

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
BOOL
MainWindow::PeekAndPump(void)
{
    MSG msg;

    while (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
        if (!AfxGetApp()->PumpMessage()) {
            ::PostQuitMessage(0);
            return FALSE;
        }
    }

    LONG lIdle = 0;
    while (AfxGetApp()->OnIdle(lIdle++))
        ;
    return TRUE;
}


/*
 * ==========================================================================
 *      Disk image processing
 * ==========================================================================
 */

/*
 * Handle a debug message from the DiskImg library.
 */
/*static*/ void
MainWindow::DebugMsgHandler(const char* file, int line, const char* msg)
{
    ASSERT(file != nil);
    ASSERT(msg != nil);

#if defined(_DEBUG_LOG)
    //fprintf(gLog, "%s(%d) : %s", file, line, msg);
    fprintf(gLog, "%05u %s", gPid, msg);
#elif defined(_DEBUG)
    _CrtDbgReport(_CRT_WARN, file, line, NULL, "%s", msg);
#else
    /* do nothing */
#endif
}
/*
 * Handle a global error message from the NufxLib library.
 */
/*static*/ NuResult
MainWindow::NufxErrorMsgHandler(NuArchive* /*pArchive*/, void* vErrorMessage)
{
    const NuErrorMessage* pErrorMessage = (const NuErrorMessage*) vErrorMessage;

#if defined(_DEBUG_LOG)
    if (pErrorMessage->isDebug) {
        fprintf(gLog, "%05u <nufxlib> [D] %s\n", gPid, pErrorMessage->message);
    } else {
        fprintf(gLog, "%05u <nufxlib> %s\n", gPid, pErrorMessage->message);
    }
#elif defined(_DEBUG)
    if (pErrorMessage->isDebug) {
        _CrtDbgReport(_CRT_WARN, pErrorMessage->file, pErrorMessage->line,
            NULL, "<nufxlib> [D] %s\n", pErrorMessage->message);
    } else {
        _CrtDbgReport(_CRT_WARN, pErrorMessage->file, pErrorMessage->line,
            NULL, "<nufxlib> %s\n", pErrorMessage->message);
    }
#else
    /* do nothing */
#endif

    return kNuOK;
}

const int kLocalFssep = '\\';

typedef struct ScanOpts {
    FILE*           outfp;
    ProgressDlg*    pProgress;
} ScanOpts;

/*
 * Scan a set of files.
 */
void
MainWindow::ScanFiles(void)
{
    ChooseFilesDlg chooseFiles;
    ScanOpts scanOpts;
    char curDir[MAX_PATH] = "";
    CString errMsg;
    CString outPath;
    bool doResetDir = false;

    memset(&scanOpts, 0, sizeof(scanOpts));

    /* choose input files */
    chooseFiles.DoModal();
    if (chooseFiles.GetExitStatus() != IDOK)
        return;

    const char* buf = chooseFiles.GetFileNames();
    WMSG2("Selected path = '%s' (offset=%d)\n", buf,
        chooseFiles.GetFileNameOffset());

    /* choose output file */
    CFileDialog dlg(FALSE, _T("txt"), NULL,
        OFN_OVERWRITEPROMPT|OFN_NOREADONLYRETURN,
        "Text Files (*.txt)|*.txt|All Files (*.*)|*.*||", this);

    dlg.m_ofn.lpstrTitle = "Save Output As...";
    strcpy(dlg.m_ofn.lpstrFile, "mdc-out.txt");

    if (dlg.DoModal() != IDOK) {
        goto bail;
    }

    outPath = dlg.GetPathName();
    WMSG1("NEW FILE '%s'\n", (LPCTSTR) outPath);

    scanOpts.outfp = fopen(outPath, "w");
    if (scanOpts.outfp == nil) {
        ShowFailureMsg(this, "Unable to open output file", IDS_FAILED);
        goto bail;
    }

    long major, minor, bug;
    DiskImgLib::Global::GetVersion(&major, &minor, &bug);
    fprintf(scanOpts.outfp, "MDC for Windows v%d.%d.%d (DiskImg library v%ld.%ld.%ld)\n",
        kAppMajorVersion, kAppMinorVersion, kAppBugVersion,
        major, minor, bug);
    fprintf(scanOpts.outfp,
        "Copyright (C) 2006 by faddenSoft, LLC.  All rights reserved.\n");
    fprintf(scanOpts.outfp,
        "MDC is part of CiderPress, available from http://www.faddensoft.com/.\n");
    NuGetVersion(&major, &minor, &bug, NULL, NULL);
    fprintf(scanOpts.outfp,
        "Linked against NufxLib v%ld.%ld.%ld and zlib v%s\n",
        major, minor, bug, zlibVersion());
    fprintf(scanOpts.outfp, "\n");

    /* change to base directory */
    if (GetCurrentDirectory(sizeof(curDir), curDir) == 0) {
        errMsg = "Unable to get current directory.";
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }
    if (SetCurrentDirectory(buf) == false) {
        errMsg.Format("Unable to set current directory to '%s'.", buf);
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }
    doResetDir = true;

    time_t now;
    now = time(nil);
    fprintf(scanOpts.outfp,
        "Run started at %.24s in '%s'\n\n", ctime(&now), buf);

    /* obstruct input to the main window */
    EnableWindow(FALSE);


    /* create a modeless dialog with a cancel button */
    scanOpts.pProgress = new ProgressDlg;
    if (scanOpts.pProgress == nil)
        goto bail;
    scanOpts.pProgress->fpCancelFlag = &fCancelFlag;
    fCancelFlag = false;
    if (scanOpts.pProgress->Create(this) == FALSE) {
        WMSG0("WARNING: ProgressDlg init failed\n");
        ASSERT(false);
    } else {
        scanOpts.pProgress->CenterWindow(this);
    }

    time_t start, end;
    start = time(nil);

    /* start cranking */
    buf += chooseFiles.GetFileNameOffset();
    while (*buf != '\0') {
        if (Process(buf, &scanOpts, &errMsg) != 0) {
            WMSG2("Skipping '%s': %s.\n", buf, (LPCTSTR) errMsg);
        }

        if (fCancelFlag) {
            WMSG0("CANCELLED by user\n");
            MessageBox("Cancelled!", "MDC", MB_OK);
            goto bail;
        }

        buf += strlen(buf)+1;
    }
    end = time(nil);
    fprintf(scanOpts.outfp, "\nScan completed in %ld seconds.\n",
        end - start);

    {
        SetWindowText(_T("MDC Done!"));

        CString doneMsg;
        CString appName;

        appName.LoadString(IDS_APP_TITLE);
#ifdef _DEBUG_LOG
        doneMsg.Format("Processing completed.\r\n\r\n"
                       "Output is in '%s', log messages in '%s'.",
            outPath, kDebugLog);
#else
        doneMsg.Format("Processing completed.");
#endif
        scanOpts.pProgress->MessageBox(doneMsg, appName, MB_OK|MB_ICONINFORMATION);
    }

bail:
    if (scanOpts.outfp != nil)
        fclose(scanOpts.outfp);

    if (doResetDir && SetCurrentDirectory(curDir) == false) {
        errMsg.Format("Unable to reset current directory to '%s'.\n", curDir);
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        // bummer
    }

    // restore the main window
    EnableWindow(TRUE);

    if (scanOpts.pProgress != nil)
        scanOpts.pProgress->DestroyWindow();

    SetWindowText(_T("MDC"));
}

/*
 * Directory structure and functions, based on zDIR in Info-Zip sources.
 */
typedef struct Win32dirent {
    char    d_attr;
    char    d_name[MAX_PATH];
    int     d_first;
    HANDLE  d_hFindFile;
} Win32dirent;

static const char* kWildMatchAll = "*.*";

/*
 * Prepare a directory for reading.
 *
 * Allocates a Win32dirent struct that must be freed by the caller.
 */
Win32dirent*
MainWindow::OpenDir(const char* name)
{
    Win32dirent* dir = nil;
    char* tmpStr = nil;
    char* cp;
    WIN32_FIND_DATA fnd;

    dir = (Win32dirent*) malloc(sizeof(*dir));
    tmpStr = (char*) malloc(strlen(name) + (2 + sizeof(kWildMatchAll)));
    if (dir == nil || tmpStr == nil)
        goto failed;

    strcpy(tmpStr, name);
    cp = tmpStr + strlen(tmpStr);

    /* don't end in a colon (e.g. "C:") */
    if ((cp - tmpStr) > 0 && strrchr(tmpStr, ':') == (cp - 1))
        *cp++ = '.';
    /* must end in a slash */
    if ((cp - tmpStr) > 0 &&
            strrchr(tmpStr, kLocalFssep) != (cp - 1))
        *cp++ = kLocalFssep;

    strcpy(cp, kWildMatchAll);

    dir->d_hFindFile = FindFirstFile(tmpStr, &fnd);
    if (dir->d_hFindFile == INVALID_HANDLE_VALUE)
        goto failed;

    strcpy(dir->d_name, fnd.cFileName);
    dir->d_attr = (unsigned char) fnd.dwFileAttributes;
    dir->d_first = 1;

bail:
    free(tmpStr);
    return dir;

failed:
    free(dir);
    dir = nil;
    goto bail;
}

/*
 * Get an entry from an open directory.
 *
 * Returns a nil pointer after the last entry has been read.
 */
Win32dirent*
MainWindow::ReadDir(Win32dirent* dir)
{
    if (dir->d_first)
        dir->d_first = 0;
    else {
        WIN32_FIND_DATA fnd;

        if (!FindNextFile(dir->d_hFindFile, &fnd))
            return nil;
        strcpy(dir->d_name, fnd.cFileName);
        dir->d_attr = (unsigned char) fnd.dwFileAttributes;
    }

    return dir;
}

/*
 * Close a directory.
 */
void
MainWindow::CloseDir(Win32dirent* dir)
{
    if (dir == nil)
        return;

    FindClose(dir->d_hFindFile);
    free(dir);
}

/* might as well blend in with the UNIX version */
#define DIR_NAME_LEN(dirent)    ((int)strlen((dirent)->d_name))


/*
 * Process a file or directory.  These are expected to be names of files in
 * the current directory.
 *
 * Returns 0 on success, nonzero on error with a message in "*pErrMsg".
 */
int
MainWindow::Process(const char* pathname, ScanOpts* pScanOpts,
    CString* pErrMsg)
{
    bool exists, isDir, isReadable;
    struct stat sb;
    int result = -1;

    if (fCancelFlag)
        return -1;

    ASSERT(pathname != nil);
    ASSERT(pErrMsg != nil);

    PathName checkPath(pathname);
    int ierr = checkPath.CheckFileStatus(&sb, &exists, &isReadable, &isDir);
    if (ierr != 0) {
        pErrMsg->Format("Unexpected error while examining '%s': %s", pathname,
            strerror(ierr));
        goto bail;
    }

    if (!exists) {
        pErrMsg->Format("Couldn't find '%s'", pathname);
        goto bail;
    }
    if (!isReadable) {
        pErrMsg->Format("File '%s' isn't readable", pathname);
        goto bail;
    }
    if (isDir) {
        result = ProcessDirectory(pathname, pScanOpts, pErrMsg);
        goto bail;
    }

    (void) ScanDiskImage(pathname, pScanOpts);

    result = 0;

bail:
    if (result != 0 && pErrMsg->IsEmpty()) {
        pErrMsg->Format("Unable to add file '%s'", pathname);
    }
    return result;
}

/*
 * Win32 recursive directory descent.  Scan the contents of a directory.
 * If a subdirectory is found, follow it; otherwise, call Win32AddFile to
 * add the file.
 */
int
MainWindow::ProcessDirectory(const char* dirName, ScanOpts* pScanOpts,
    CString* pErrMsg)
{
    Win32dirent* dirp = nil;
    Win32dirent* entry;
    char nbuf[MAX_PATH];    /* malloc might be better; this soaks stack */
    char fssep;
    int len;
    int result = -1;

    ASSERT(dirName != nil);
    ASSERT(pErrMsg != nil);

    WMSG1("+++ DESCEND: '%s'\n", dirName);

    dirp = OpenDir(dirName);
    if (dirp == nil) {
        pErrMsg->Format("Failed on '%s': %s", dirName, strerror(errno));
        goto bail;
    }

    fssep = kLocalFssep;

    /* could use readdir_r, but we don't care about reentrancy here */
    while ((entry = ReadDir(dirp)) != nil) {
        /* skip the dotsies */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        len = strlen(dirName);
        if (len + DIR_NAME_LEN(entry) +2 > MAX_PATH) {
            WMSG4("ERROR: Filename exceeds %d bytes: %s%c%s",
                MAX_PATH, dirName, fssep, entry->d_name);
            goto bail;
        }

        /* form the new name, inserting an fssep if needed */
        strcpy(nbuf, dirName);
        if (dirName[len-1] != fssep)
            nbuf[len++] = fssep;
        strcpy(nbuf+len, entry->d_name);

        result = Process(nbuf, pScanOpts, pErrMsg);
        if (result != 0)
            goto bail;
    }

    result = 0;

bail:
    if (dirp != nil)
        (void)CloseDir(dirp);
    return result;
}


/*
 * Open a disk image and dump the contents.
 *
 * Returns 0 on success, nonzero on failure.
 */
int
MainWindow::ScanDiskImage(const char* pathName, ScanOpts* pScanOpts)
{
    ASSERT(pathName != nil);
    ASSERT(pScanOpts != nil);
    ASSERT(pScanOpts->outfp != nil);

    DIError dierr;
    CString errMsg;
    DiskImg diskImg;
    DiskFS* pDiskFS = nil;
    PathName path(pathName);
    CString ext = path.GetExtension();

    /* first, some housekeeping */
    PeekAndPump();
    pScanOpts->pProgress->SetCurrentFile(pathName);
    if (fCancelFlag)
        return -1;

    CString title;
    title = _T("MDC ");
    title += FilenameOnly(pathName, '\\');
    SetWindowText(title);

    fprintf(pScanOpts->outfp, "File: %s\n", pathName);
    fflush(pScanOpts->outfp);       // in case we crash

    if (!ext.IsEmpty()) {
        /* delete the leading '.' */
        ext.Delete(0, 1);
    }

    dierr = diskImg.OpenImage(pathName, '\\', true);
    if (dierr != kDIErrNone) {
        errMsg.Format("Unable to open '%s': %s", pathName,
            DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    dierr = diskImg.AnalyzeImage();
    if (dierr != kDIErrNone) {
        errMsg.Format("Analysis of '%s' failed: %s", pathName,
            DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    if (diskImg.GetFSFormat() == DiskImg::kFormatUnknown ||
        diskImg.GetSectorOrder() == DiskImg::kSectorOrderUnknown)
    {
        errMsg.Format("Unable to identify filesystem on '%s'", pathName);
        goto bail;
    }

    /* create an appropriate DiskFS object */
    pDiskFS = diskImg.OpenAppropriateDiskFS();
    if (pDiskFS == nil) {
        /* unknown FS should've been caught above! */
        ASSERT(false);
        errMsg.Format("Format of '%s' not recognized.", pathName);
        goto bail;
    }

    pDiskFS->SetScanForSubVolumes(DiskFS::kScanSubEnabled);

    /* object created; prep it */
    dierr = pDiskFS->Initialize(&diskImg, DiskFS::kInitFull);
    if (dierr != kDIErrNone) {
        errMsg.Format("Error reading list of files from disk: %s",
            DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    int kbytes;
    if (pDiskFS->GetDiskImg()->GetHasBlocks())
        kbytes = pDiskFS->GetDiskImg()->GetNumBlocks() / 2;
    else if (pDiskFS->GetDiskImg()->GetHasSectors())
        kbytes = (pDiskFS->GetDiskImg()->GetNumTracks() *
                pDiskFS->GetDiskImg()->GetNumSectPerTrack()) / 4;
    else
        kbytes = 0;
    fprintf(pScanOpts->outfp, "Disk: %s%s (%dKB)\n", pDiskFS->GetVolumeID(),
        pDiskFS->GetFSDamaged() ? " [*]" : "", kbytes);
    fprintf(pScanOpts->outfp,
        " Name                             Type Auxtyp Modified"
        "         Format   Length\n");
    fprintf(pScanOpts->outfp,
        "------------------------------------------------------"
        "------------------------\n");
    if (LoadDiskFSContents(pDiskFS, "", pScanOpts) != 0) {
        errMsg.Format("Failed while loading contents of '%s'.", pathName);
        goto bail;
    }
    fprintf(pScanOpts->outfp,
        "------------------------------------------------------"
        "------------------------\n\n");

bail:
    delete pDiskFS;

    //PeekAndPump();

    if (!errMsg.IsEmpty()) {
        fprintf(pScanOpts->outfp, "Failed: %s\n\n", (LPCTSTR) errMsg);
        return -1;
    } else {
        return 0;
    }
}

/*
 * Analyze a file's characteristics.
 */
void
MainWindow::AnalyzeFile(const A2File* pFile, RecordKind* pRecordKind,
    LONGLONG* pTotalLen, LONGLONG* pTotalCompLen)
{
    if (pFile->IsVolumeDirectory()) {
        /* volume dir entry */
        ASSERT(pFile->GetRsrcLength() < 0);
        *pRecordKind = kRecordKindVolumeDir;
        *pTotalLen = pFile->GetDataLength();
        *pTotalCompLen = pFile->GetDataLength();
    } else if (pFile->IsDirectory()) {
        /* directory entry */
        ASSERT(pFile->GetRsrcLength() < 0);
        *pRecordKind = kRecordKindDirectory;
        *pTotalLen = pFile->GetDataLength();
        *pTotalCompLen = pFile->GetDataLength();
    } else if (pFile->GetRsrcLength() >= 0) {
        /* has resource fork */
        *pRecordKind = kRecordKindForkedFile;
        *pTotalLen = pFile->GetDataLength() + pFile->GetRsrcLength();
        *pTotalCompLen =
            pFile->GetDataSparseLength() + pFile->GetRsrcSparseLength();
    } else {
        /* just data fork */
        *pRecordKind = kRecordKindFile;
        *pTotalLen = pFile->GetDataLength();
        *pTotalCompLen = pFile->GetDataSparseLength();
    }
}

/*
 * Determine whether the access bits on the record make it a read-only
 * file or not.
 *
 * Uses a simplified view of the access flags.
 */
bool
MainWindow::IsRecordReadOnly(int access)
{
    if (access == 0x21L || access == 0x01L)
        return true;
    else
        return false;
}

/* ProDOS file type names; must be entirely in upper case */
static const char gFileTypeNames[256][4] = {
    "NON", "BAD", "PCD", "PTX", "TXT", "PDA", "BIN", "FNT",
    "FOT", "BA3", "DA3", "WPF", "SOS", "$0D", "$0E", "DIR",
    "RPD", "RPI", "AFD", "AFM", "AFR", "SCL", "PFS", "$17",
    "$18", "ADB", "AWP", "ASP", "$1C", "$1D", "$1E", "$1F",
    "TDM", "$21", "$22", "$23", "$24", "$25", "$26", "$27",
    "$28", "$29", "8SC", "8OB", "8IC", "8LD", "P8C", "$2F",
    "$30", "$31", "$32", "$33", "$34", "$35", "$36", "$37",
    "$38", "$39", "$3A", "$3B", "$3C", "$3D", "$3E", "$3F",
    "DIC", "OCR", "FTD", "$43", "$44", "$45", "$46", "$47",
    "$48", "$49", "$4A", "$4B", "$4C", "$4D", "$4E", "$4F",
    "GWP", "GSS", "GDB", "DRW", "GDP", "HMD", "EDU", "STN",
    "HLP", "COM", "CFG", "ANM", "MUM", "ENT", "DVU", "FIN",
    "$60", "$61", "$62", "$63", "$64", "$65", "$66", "$67",
    "$68", "$69", "$6A", "BIO", "$6C", "TDR", "PRE", "HDV",
    "$70", "$71", "$72", "$73", "$74", "$75", "$76", "$77",
    "$78", "$79", "$7A", "$7B", "$7C", "$7D", "$7E", "$7F",
    "$80", "$81", "$82", "$83", "$84", "$85", "$86", "$87",
    "$88", "$89", "$8A", "$8B", "$8C", "$8D", "$8E", "$8F",
    "$90", "$91", "$92", "$93", "$94", "$95", "$96", "$97",
    "$98", "$99", "$9A", "$9B", "$9C", "$9D", "$9E", "$9F",
    "WP ", "$A1", "$A2", "$A3", "$A4", "$A5", "$A6", "$A7",
    "$A8", "$A9", "$AA", "GSB", "TDF", "BDF", "$AE", "$AF",
    "SRC", "OBJ", "LIB", "S16", "RTL", "EXE", "PIF", "TIF",
    "NDA", "CDA", "TOL", "DVR", "LDF", "FST", "$BE", "DOC",
    "PNT", "PIC", "ANI", "PAL", "$C4", "OOG", "SCR", "CDV",
    "FON", "FND", "ICN", "$CB", "$CC", "$CD", "$CE", "$CF",
    "$D0", "$D1", "$D2", "$D3", "$D4", "MUS", "INS", "MDI",
    "SND", "$D9", "$DA", "DBM", "$DC", "DDD", "$DE", "$DF",
    "LBR", "$E1", "ATK", "$E3", "$E4", "$E5", "$E6", "$E7",
    "$E8", "$E9", "$EA", "$EB", "$EC", "$ED", "R16", "PAS",
    "CMD", "$F1", "$F2", "$F3", "$F4", "$F5", "$F6", "$F7",
    "$F8", "OS ", "INT", "IVR", "BAS", "VAR", "REL", "SYS"
};

/*
 * Return a pointer to the three-letter representation of the file type name.
 *
 * Note to self: code down below tests first char for '?'.
 */
/*static*/ const char*
MainWindow::GetFileTypeString(unsigned long fileType)
{
    if (fileType < NELEM(gFileTypeNames))
        return gFileTypeNames[fileType];
    else
        return "???";
}

/*  
 * Sanitize a string.  The Mac likes to stick control characters into
 * things, e.g. ^C and ^M, and uses high ASCII for special characters.
 */
static void
MacSanitize(char* str)
{       
    while (*str != '\0') {
        *str = DiskImg::MacToASCII(*str);
        str++;
    }
}   

/*
 * Load the contents of a DiskFS.
 *
 * Recursively handle sub-volumes.
 */
int
MainWindow::LoadDiskFSContents(DiskFS* pDiskFS, const char* volName,
    ScanOpts* pScanOpts)
{
    static const char* kBlankFileName = "<blank filename>";
    DiskFS::SubVolume* pSubVol = nil;
    A2File* pFile;

    ASSERT(pDiskFS != nil);
    pFile = pDiskFS->GetNextFile(nil);
    for ( ; pFile != nil; pFile = pDiskFS->GetNextFile(pFile)) {
        CString subVolName, dispName;
        RecordKind recordKind;
        LONGLONG totalLen, totalCompLen;
        char tmpbuf[16];

        AnalyzeFile(pFile, &recordKind, &totalLen, &totalCompLen);

        if (recordKind == kRecordKindVolumeDir) {
            /* this is a volume directory */
            WMSG1("Not displaying volume dir '%s'\n", pFile->GetPathName());
            continue;
        }

        /* prepend volName for sub-volumes; must be valid Win32 dirname */
        if (volName[0] != '\0')
            subVolName.Format("_%s", volName);

        const char* ccp = pFile->GetPathName();
        ASSERT(ccp != nil);
        if (strlen(ccp) == 0)
            ccp = kBlankFileName;

        CString path(ccp);
        if (DiskImg::UsesDOSFileStructure(pFile->GetFSFormat()) && 0)
        {
            InjectLowercase(&path);
        }

        if (subVolName.IsEmpty())
            dispName = path;
        else {
            dispName = subVolName;
            dispName += ':';
            dispName += path;
        }

        /* strip out ctrl chars and high ASCII in HFS names */
        MacSanitize(dispName.GetBuffer(0));
        dispName.ReleaseBuffer();

        ccp = dispName;

        int len = strlen(ccp);
        if (len <= 32) {
            fprintf(pScanOpts->outfp, "%c%-32.32s ",
                IsRecordReadOnly(pFile->GetAccess()) ? '*' : ' ',
                ccp);
        } else {
            fprintf(pScanOpts->outfp, "%c..%-30.30s ",
                IsRecordReadOnly(pFile->GetAccess()) ? '*' : ' ',
                ccp + len - 30);
        }
        switch (recordKind) {
        case kRecordKindUnknown:
            fprintf(pScanOpts->outfp, "%s- $%04lX  ",
                GetFileTypeString(pFile->GetFileType()),
                pFile->GetAuxType());
            break;
        case kRecordKindDisk:
            sprintf(tmpbuf, "%ldk", totalLen / 1024);
            fprintf(pScanOpts->outfp, "Disk %-6s ", tmpbuf);
            break;
        case kRecordKindFile:
        case kRecordKindForkedFile:
        case kRecordKindDirectory:
            if (pDiskFS->GetDiskImg()->GetFSFormat() == DiskImg::kFormatMacHFS)
            {
                if (recordKind != kRecordKindDirectory &&
                    pFile->GetFileType() >= 0 && pFile->GetFileType() <= 0xff &&
                    pFile->GetAuxType() >= 0 && pFile->GetAuxType() <= 0xffff)
                {
                    /* ProDOS type embedded in HFS */
                    fprintf(pScanOpts->outfp, "%s%c $%04lX  ",
                        GetFileTypeString(pFile->GetFileType()),
                        recordKind == kRecordKindForkedFile ? '+' : ' ',
                        pFile->GetAuxType());
                } else {
                    char typeStr[5];
                    char creatorStr[5];
                    unsigned long val;

                    val = pFile->GetAuxType();
                    creatorStr[0] = (unsigned char) (val >> 24);
                    creatorStr[1] = (unsigned char) (val >> 16);
                    creatorStr[2] = (unsigned char) (val >> 8);
                    creatorStr[3] = (unsigned char) val;
                    creatorStr[4] = '\0';

                    val = pFile->GetFileType();
                    typeStr[0] = (unsigned char) (val >> 24);
                    typeStr[1] = (unsigned char) (val >> 16);
                    typeStr[2] = (unsigned char) (val >> 8);
                    typeStr[3] = (unsigned char) val;
                    typeStr[4] = '\0';

                    MacSanitize(creatorStr);
                    MacSanitize(typeStr);

                    if (recordKind == kRecordKindDirectory) {
                        fprintf(pScanOpts->outfp, "DIR   %-4s  ", creatorStr);
                    } else {
                        fprintf(pScanOpts->outfp, "%-4s%c %-4s  ",
                            typeStr,
                            pFile->GetRsrcLength() > 0 ? '+' : ' ',
                            creatorStr);
                    }
                }
            } else {
                fprintf(pScanOpts->outfp, "%s%c $%04lX  ",
                    GetFileTypeString(pFile->GetFileType()),
                    recordKind == kRecordKindForkedFile ? '+' : ' ',
                    pFile->GetAuxType());
            }
            break;
        case kRecordKindVolumeDir:
            /* should've trapped this earlier */
            ASSERT(0);
            fprintf(pScanOpts->outfp, "ERROR  ");
            break;
        default:
            ASSERT(0);
            fprintf(pScanOpts->outfp, "ERROR  ");
            break;
        }

        CString date;
        if (pFile->GetModWhen() == 0)
            FormatDate(kDateNone, &date);
        else
            FormatDate(pFile->GetModWhen(), &date);
        fprintf(pScanOpts->outfp, "%-15s  ", (LPCTSTR) date);

        const char* fmtStr;
        switch (pFile->GetFSFormat()) {
        case DiskImg::kFormatDOS33:
        case DiskImg::kFormatDOS32:
        case DiskImg::kFormatUNIDOS:
        case DiskImg::kFormatOzDOS:
            fmtStr = "DOS   ";
            break;
        case DiskImg::kFormatProDOS:
            fmtStr = "ProDOS";
            break;
        case DiskImg::kFormatPascal:
            fmtStr = "Pascal";
            break;
        case DiskImg::kFormatCPM:
            fmtStr = "CP/M  ";
            break;
        case DiskImg::kFormatRDOS33:
        case DiskImg::kFormatRDOS32:
        case DiskImg::kFormatRDOS3:
            fmtStr = "RDOS  ";
            break;
        case DiskImg::kFormatMacHFS:
            fmtStr = "HFS   ";
            break;
        case DiskImg::kFormatMSDOS:
            fmtStr = "MS-DOS";
            break;
        default:
            fmtStr = "???   ";
            break;
        }
        if (pFile->GetQuality() == A2File::kQualityDamaged)
            fmtStr = "BROKEN";
        else if (pFile->GetQuality() == A2File::kQualitySuspicious)
            fmtStr = "BAD?  ";

        fprintf(pScanOpts->outfp, "%s ", fmtStr);


#if 0
        /* compute the percent size */
        if ((!totalLen && totalCompLen) || (totalLen && !totalCompLen))
            fprintf(pScanOpts->outfp, "---   ");       /* weird */
        else if (totalLen < totalCompLen)
            fprintf(pScanOpts->outfp, ">100%% ");      /* compression failed? */
        else {
            sprintf(tmpbuf, "%02d%%", ComputePercent(totalCompLen, totalLen));
            fprintf(pScanOpts->outfp, "%4s  ", tmpbuf);
        }
#endif

        if (!totalLen && totalCompLen)
            fprintf(pScanOpts->outfp, "   ????");      /* weird */
        else
            fprintf(pScanOpts->outfp, "%8ld", totalLen);

        fprintf(pScanOpts->outfp, "\n");
    }

    /*
     * Load all sub-volumes.
     */
    pSubVol = pDiskFS->GetNextSubVolume(nil);
    while (pSubVol != nil) {
        const char* subVolName;
        int ret;

        subVolName = pSubVol->GetDiskFS()->GetVolumeName();
        if (subVolName == nil)
            subVolName = "+++";     // could probably do better than this

        ret = LoadDiskFSContents(pSubVol->GetDiskFS(), subVolName, pScanOpts);
        if (ret != 0)
            return ret;
        pSubVol = pDiskFS->GetNextSubVolume(pSubVol);
    }

    return 0;
}
