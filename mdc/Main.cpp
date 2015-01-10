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
#include "ProgressDlg.h"
#include "resource.h"
#include "../diskimg/DiskImg.h"
#include "../zlib/zlib.h"

const WCHAR* kWebSiteURL = L"http://www.a2ciderpress.com/";


BEGIN_MESSAGE_MAP(MainWindow, CFrameWnd)
    ON_COMMAND(IDM_FILE_SCAN, OnFileScan)
    ON_COMMAND(IDM_FILE_EXIT, OnFileExit)
    ON_COMMAND(IDM_HELP_WEBSITE, OnHelpWebSite)
    ON_COMMAND(IDM_HELP_ABOUT, OnHelpAbout)
END_MESSAGE_MAP()


MainWindow::MainWindow()
{
    static const WCHAR* kAppName = L"MDC";

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

MainWindow::~MainWindow()
{
    DiskImgLib::Global::AppCleanup();
}

void MainWindow::OnFileExit(void)
{
    // Handle Exit item by sending a close request.
    SendMessage(WM_CLOSE, 0, 0);
}

void MainWindow::OnHelpWebSite(void)
{
    // Go to the CiderPress web site.
    int err;

    err = (int) ::ShellExecute(m_hWnd, L"open", kWebSiteURL, NULL, NULL,
                    SW_SHOWNORMAL);
    if (err <= 32) {
        CString msg;
        msg.Format(L"Unable to launch web browser (err=%d).", err);
        ShowFailureMsg(this, msg, IDS_FAILED);
    }
}

void MainWindow::OnHelpAbout(void)
{
    int result;

    AboutDlg dlg(this);

    result = dlg.DoModal();
    LOGI("HelpAbout returned %d", result);
}

void MainWindow::OnFileScan(void)
{
    if (0) {
        CString msg;
        CheckedLoadString(&msg, IDS_MUST_REGISTER);
        ShowFailureMsg(this, msg, IDS_APP_TITLE);
    } else {
        ScanFiles();
    }
}

BOOL MainWindow::PeekAndPump(void)
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

/*static*/ void MainWindow::DebugMsgHandler(const char* file, int line,
    const char* msg)
{
    ASSERT(file != NULL);
    ASSERT(msg != NULL);

    LOG_BASE(DebugLog::LOG_INFO, file, line, "<diskimg> %hs", msg);
}

/*static*/ NuResult MainWindow::NufxErrorMsgHandler(NuArchive* /*pArchive*/,
    void* vErrorMessage)
{
    const NuErrorMessage* pErrorMessage = (const NuErrorMessage*) vErrorMessage;

    LOG_BASE(pErrorMessage->isDebug ? DebugLog::LOG_DEBUG : DebugLog::LOG_WARN,
        pErrorMessage->file, pErrorMessage->line, "<nufxlib> %hs",
        pErrorMessage->message);

    return kNuOK;
}


const int kLocalFssep = '\\';

struct ScanOpts {
    FILE*           outfp;
    ProgressDlg*    pProgress;
};

void MainWindow::ScanFiles(void)
{
    WCHAR curDir[MAX_PATH] = L"";
    CString errMsg, newDir;
    bool doResetDir = false;
    ScanOpts scanOpts;

    memset(&scanOpts, 0, sizeof(scanOpts));

    // choose input files
    SelectFilesDialog chooseFiles(L"IDD_CHOOSE_FILES", false, this);
    chooseFiles.SetWindowTitle(L"Choose Files...");
    INT_PTR retval = chooseFiles.DoModal();
    if (retval != IDOK) {
        return;
    }

    // choose output file; use an Explorer-style dialog for consistency
    CString outPath;
    CFileDialog dlg(FALSE, L"txt", NULL,
        OFN_OVERWRITEPROMPT|OFN_NOREADONLYRETURN,
        L"Text Files (*.txt)|*.txt|All Files (*.*)|*.*||", this,
        0, FALSE /*disable Vista style*/);

    dlg.m_ofn.lpstrTitle = L"Save Output As...";
    wcscpy(dlg.m_ofn.lpstrFile, L"mdc-out.txt");

    if (dlg.DoModal() != IDOK) {
        goto bail;
    }

    outPath = dlg.GetPathName();
    LOGI("NEW FILE '%ls'", (LPCWSTR) outPath);

    scanOpts.outfp = _wfopen(outPath, L"w");
    if (scanOpts.outfp == NULL) {
        ShowFailureMsg(this, "Unable to open output file", IDS_FAILED);
        goto bail;
    }

    int32_t major, minor, bug;
    DiskImgLib::Global::GetVersion(&major, &minor, &bug);
    fprintf(scanOpts.outfp, "MDC for Windows v%d.%d.%d (DiskImg library v%ld.%ld.%ld)\n",
        kAppMajorVersion, kAppMinorVersion, kAppBugVersion,
        major, minor, bug);
    fprintf(scanOpts.outfp,
        "Copyright (C) 2014 by faddenSoft, LLC.  All rights reserved.\n");
    fprintf(scanOpts.outfp,
        "MDC is part of CiderPress, available from http://www.a2ciderpress.com/.\n");
    NuGetVersion(&major, &minor, &bug, NULL, NULL);
    fprintf(scanOpts.outfp,
        "Linked against NufxLib v%ld.%ld.%ld and zlib v%hs\n",
        major, minor, bug, zlibVersion());
    fprintf(scanOpts.outfp, "\n");

    /* change to base directory */
    if (GetCurrentDirectory(NELEM(curDir), curDir) == 0) {
        errMsg = L"Unable to get current directory.";
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }
    newDir = chooseFiles.GetDirectory();
    if (SetCurrentDirectory(newDir) == false) {
        errMsg.Format(L"Unable to change current directory to '%ls'.",
            (LPCWSTR) newDir);
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }
    doResetDir = true;

    time_t now;
    now = time(NULL);
    fprintf(scanOpts.outfp,
        "Run started at %.24hs in '%ls'\n\n", ctime(&now), (LPCWSTR) newDir);

    /* obstruct input to the main window */
    EnableWindow(FALSE);


    /* create a modeless dialog with a cancel button */
    scanOpts.pProgress = new ProgressDlg;
    if (scanOpts.pProgress == NULL)
        goto bail;
    scanOpts.pProgress->fpCancelFlag = &fCancelFlag;
    fCancelFlag = false;
    if (scanOpts.pProgress->Create(this) == FALSE) {
        LOGI("WARNING: ProgressDlg init failed");
        ASSERT(false);
    } else {
        scanOpts.pProgress->CenterWindow(this);
    }

    time_t start, end;
    start = time(NULL);

    /* start cranking */
    const CStringArray& arr = chooseFiles.GetFileNames();
    for (int i = 0; i < arr.GetCount(); i++) {
        const CString& name = arr.GetAt(i);
        if (Process(name, &scanOpts, &errMsg) != 0) {
            LOGI("Skipping '%ls': %ls.", (LPCWSTR) name, (LPCWSTR) errMsg);
        }

        if (fCancelFlag) {
            LOGI("Canceled by user");
            MessageBox(L"Canceled!", L"MDC", MB_OK);
            goto bail;
        }
    }
    end = time(NULL);
    fprintf(scanOpts.outfp, "\nScan completed in %ld seconds.\n",
        (long) (end - start));

    {
        SetWindowText(L"MDC Done!");

        CString doneMsg = L"Processing completed.";
        CString appName;

        CheckedLoadString(&appName, IDS_APP_TITLE);
        scanOpts.pProgress->MessageBox(doneMsg, appName, MB_OK|MB_ICONINFORMATION);
    }

bail:
    if (scanOpts.outfp != NULL)
        fclose(scanOpts.outfp);

    if (doResetDir && SetCurrentDirectory(curDir) == false) {
        errMsg.Format(L"Unable to reset current directory to '%ls'.\n", curDir);
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        // bummer
    }

    // restore the main window
    EnableWindow(TRUE);

    if (scanOpts.pProgress != NULL)
        scanOpts.pProgress->DestroyWindow();

    SetWindowText(L"MDC");
}

/*
 * Directory structure and functions, modified from zDIR in Info-Zip sources.
 */
typedef struct Win32dirent {
    DWORD   d_attr;
    WCHAR   d_name[MAX_PATH];
    int     d_first;
    HANDLE  d_hFindFile;
} Win32dirent;

static const WCHAR kWildMatchAll[] = L"*.*";

Win32dirent* MainWindow::OpenDir(const WCHAR* name)
{
    Win32dirent* dir = NULL;
    WCHAR* tmpStr = NULL;
    WCHAR* cp;
    WIN32_FIND_DATA fnd;

    dir = (Win32dirent*) malloc(sizeof(*dir));
    tmpStr = (WCHAR*) malloc((wcslen(name) + wcslen(kWildMatchAll) + 2)
        * sizeof(WCHAR));
    if (dir == NULL || tmpStr == NULL)
        goto failed;

    wcscpy(tmpStr, name);
    cp = tmpStr + wcslen(tmpStr);

    /* don't end in a colon (e.g. "C:") */
    if ((cp - tmpStr) > 0 && wcsrchr(tmpStr, ':') == (cp - 1))
        *cp++ = '.';
    /* must end in a slash */
    if ((cp - tmpStr) > 0 &&
            wcsrchr(tmpStr, kLocalFssep) != (cp - 1))
        *cp++ = kLocalFssep;

    wcscpy(cp, kWildMatchAll);

    dir->d_hFindFile = FindFirstFile(tmpStr, &fnd);
    if (dir->d_hFindFile == INVALID_HANDLE_VALUE)
        goto failed;

    wcscpy(dir->d_name, fnd.cFileName);
    dir->d_attr = fnd.dwFileAttributes;
    dir->d_first = 1;

bail:
    free(tmpStr);
    return dir;

failed:
    free(dir);
    dir = NULL;
    goto bail;
}

Win32dirent* MainWindow::ReadDir(Win32dirent* dir)
{
    if (dir->d_first)
        dir->d_first = 0;
    else {
        WIN32_FIND_DATA fnd;

        if (!FindNextFile(dir->d_hFindFile, &fnd))
            return NULL;
        wcscpy(dir->d_name, fnd.cFileName);
        dir->d_attr = (unsigned char) fnd.dwFileAttributes;
    }

    return dir;
}

void MainWindow::CloseDir(Win32dirent* dir)
{
    if (dir == NULL)
        return;

    FindClose(dir->d_hFindFile);
    free(dir);
}

int MainWindow::Process(const WCHAR* pathname, ScanOpts* pScanOpts,
    CString* pErrMsg)
{
    bool exists, isDir, isReadable;
    struct _stat sb;
    int result = -1;

    if (fCancelFlag)
        return -1;

    ASSERT(pathname != NULL);
    ASSERT(pErrMsg != NULL);

    PathName checkPath(pathname);
    int ierr = checkPath.CheckFileStatus(&sb, &exists, &isReadable, &isDir);
    if (ierr != 0) {
        pErrMsg->Format(L"Unexpected error while examining '%ls': %hs", pathname,
            strerror(ierr));
        goto bail;
    }

    if (!exists) {
        pErrMsg->Format(L"Couldn't find '%ls'", pathname);
        goto bail;
    }
    if (!isReadable) {
        pErrMsg->Format(L"File '%ls' isn't readable", pathname);
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
        pErrMsg->Format(L"Unable to add file '%ls'", pathname);
    }
    return result;
}

int MainWindow::ProcessDirectory(const WCHAR* dirName, ScanOpts* pScanOpts,
    CString* pErrMsg)
{
    Win32dirent* dirp = NULL;
    Win32dirent* entry;
    WCHAR nbuf[MAX_PATH];   /* malloc might be better; this soaks stack */
    WCHAR fssep;
    int len;
    int result = -1;

    ASSERT(dirName != NULL);
    ASSERT(pErrMsg != NULL);

    LOGI("+++ DESCEND: '%ls'", (LPCWSTR) dirName);

    dirp = OpenDir(dirName);
    if (dirp == NULL) {
        pErrMsg->Format(L"Failed on '%ls': %hs", dirName, strerror(errno));
        goto bail;
    }

    fssep = kLocalFssep;

    /* could use readdir_r, but we don't care about reentrancy here */
    while ((entry = ReadDir(dirp)) != NULL) {
        /* skip the dotsies */
        if (wcscmp(entry->d_name, L".") == 0 || wcscmp(entry->d_name, L"..") == 0)
            continue;

        len = wcslen(dirName);
        if (len + wcslen(entry->d_name) + 2 > MAX_PATH) {
            LOGE("ERROR: Filename exceeds %d bytes: %ls%c%ls",
                MAX_PATH, dirName, fssep, entry->d_name);
            goto bail;
        }

        /* form the new name, inserting an fssep if needed */
        wcscpy(nbuf, dirName);
        if (dirName[len - 1] != fssep) {
            nbuf[len++] = fssep;
        }
        wcscpy(nbuf+len, entry->d_name);

        result = Process(nbuf, pScanOpts, pErrMsg);
        if (result != 0)
            goto bail;
    }

    result = 0;

bail:
    if (dirp != NULL)
        (void)CloseDir(dirp);
    return result;
}

int MainWindow::ScanDiskImage(const WCHAR* pathName, ScanOpts* pScanOpts)
{
    ASSERT(pathName != NULL);
    ASSERT(pScanOpts != NULL);
    ASSERT(pScanOpts->outfp != NULL);

    DIError dierr;
    CString errMsg;
    DiskImg diskImg;
    DiskFS* pDiskFS = NULL;
    PathName path(pathName);
    CString ext = path.GetExtension();

    /* first, some housekeeping */
    PeekAndPump();
    pScanOpts->pProgress->SetCurrentFile(pathName);
    if (fCancelFlag)
        return -1;

    CString title;
    title = L"MDC ";
    title += PathName::FilenameOnly(pathName, '\\');
    SetWindowText(title);

    fprintf(pScanOpts->outfp, "File: %ls\n", pathName);
    fflush(pScanOpts->outfp);       // in case we crash

    if (!ext.IsEmpty()) {
        /* delete the leading '.' */
        ext.Delete(0, 1);
    }

    CStringA pathNameA(pathName);
    dierr = diskImg.OpenImage(pathNameA, '\\', true);
    if (dierr != kDIErrNone) {
        errMsg.Format(L"Unable to open '%ls': %hs", pathName,
            DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    dierr = diskImg.AnalyzeImage();
    if (dierr != kDIErrNone) {
        errMsg.Format(L"Analysis of '%ls' failed: %hs", pathName,
            DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    if (diskImg.GetFSFormat() == DiskImg::kFormatUnknown ||
        diskImg.GetSectorOrder() == DiskImg::kSectorOrderUnknown)
    {
        errMsg.Format(L"Unable to identify filesystem on '%ls'", pathName);
        goto bail;
    }

    /* create an appropriate DiskFS object */
    pDiskFS = diskImg.OpenAppropriateDiskFS();
    if (pDiskFS == NULL) {
        /* unknown FS should've been caught above! */
        ASSERT(false);
        errMsg.Format(L"Format of '%ls' not recognized.", pathName);
        goto bail;
    }

    pDiskFS->SetScanForSubVolumes(DiskFS::kScanSubEnabled);

    /* object created; prep it */
    dierr = pDiskFS->Initialize(&diskImg, DiskFS::kInitFull);
    if (dierr != kDIErrNone) {
        errMsg.Format(L"Error reading list of files from disk: %hs",
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
    fprintf(pScanOpts->outfp, "Disk: %hs%hs (%dKB)\n", pDiskFS->GetVolumeID(),
        pDiskFS->GetFSDamaged() ? " [*]" : "", kbytes);
    fprintf(pScanOpts->outfp,
        " Name                             Type Auxtyp Modified"
        "         Format   Length\n");
    fprintf(pScanOpts->outfp,
        "------------------------------------------------------"
        "------------------------\n");
    if (LoadDiskFSContents(pDiskFS, "", pScanOpts) != 0) {
        errMsg.Format(L"Failed while loading contents of '%ls'.", pathName);
        goto bail;
    }
    fprintf(pScanOpts->outfp,
        "------------------------------------------------------"
        "------------------------\n\n");

bail:
    delete pDiskFS;

    //PeekAndPump();

    if (!errMsg.IsEmpty()) {
        fprintf(pScanOpts->outfp, "Failed: %ls\n\n", (LPCWSTR) errMsg);
        return -1;
    } else {
        return 0;
    }
}

void MainWindow::AnalyzeFile(const A2File* pFile, RecordKind* pRecordKind,
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

bool MainWindow::IsRecordReadOnly(int access)
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

/*static*/ const char* MainWindow::GetFileTypeString(unsigned long fileType)
{
    if (fileType < NELEM(gFileTypeNames))
        return gFileTypeNames[fileType];
    else
        return "???";
}

/*  
 * Sanitize a string.  The Mac likes to stick control characters into
 * things, e.g. ^C and ^M, and uses high ASCII for special characters.
 *
 * TODO(Unicode): we could do a Mac OS Roman to Unicode conversion, but
 * we'd probably want to output UTF-8 (which Windows accessories like
 * Notepad *are* able to read).
 */
static void MacSanitize(char* str)
{       
    while (*str != '\0') {
        *str = DiskImg::MacToASCII(*str);
        str++;
    }
}   

int MainWindow::LoadDiskFSContents(DiskFS* pDiskFS, const char* volName,
    ScanOpts* pScanOpts)
{
    static const char* kBlankFileNameMOR = "<blank filename>";
    DiskFS::SubVolume* pSubVol = NULL;
    A2File* pFile;

    ASSERT(pDiskFS != NULL);
    pFile = pDiskFS->GetNextFile(NULL);
    for ( ; pFile != NULL; pFile = pDiskFS->GetNextFile(pFile)) {
        CStringA subVolName, dispName;
        RecordKind recordKind;
        LONGLONG totalLen, totalCompLen;
        char tmpbuf[16];

        AnalyzeFile(pFile, &recordKind, &totalLen, &totalCompLen);

        if (recordKind == kRecordKindVolumeDir) {
            /* this is a volume directory */
            LOGI("Not displaying volume dir '%hs'", pFile->GetPathName());
            continue;
        }

        /* prepend volName for sub-volumes; must be valid Win32 dirname */
        if (volName[0] != '\0')
            subVolName.Format("_%hs", volName);

        const char* ccp = pFile->GetPathName();
        ASSERT(ccp != NULL);
        if (strlen(ccp) == 0)
            ccp = kBlankFileNameMOR;

        CStringA path(ccp);
        if (DiskImg::UsesDOSFileStructure(pFile->GetFSFormat()) && 0) {
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
        // TODO: consider having a Unicode output mode
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
            fprintf(pScanOpts->outfp, "%hs- $%04lX  ",
                GetFileTypeString(pFile->GetFileType()),
                pFile->GetAuxType());
            break;
        case kRecordKindDisk:
            sprintf(tmpbuf, "%I64dk", totalLen / 1024);
            fprintf(pScanOpts->outfp, "Disk %-6hs ", tmpbuf);
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
                    fprintf(pScanOpts->outfp, "%hs%c $%04lX  ",
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
                fprintf(pScanOpts->outfp, "%hs%c $%04lX  ",
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
        fprintf(pScanOpts->outfp, "%-15ls  ", (LPCWSTR) date);

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
        case DiskImg::kFormatGutenberg:
            fmtStr = "Gutenb";
            break;
        default:
            fmtStr = "???   ";
            break;
        }
        if (pFile->GetQuality() == A2File::kQualityDamaged)
            fmtStr = "BROKEN";
        else if (pFile->GetQuality() == A2File::kQualitySuspicious)
            fmtStr = "BAD?  ";

        fprintf(pScanOpts->outfp, "%hs ", fmtStr);


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
            fprintf(pScanOpts->outfp, "%8I64d", totalLen);

        fprintf(pScanOpts->outfp, "\n");
    }

    /*
     * Load all sub-volumes.
     */
    pSubVol = pDiskFS->GetNextSubVolume(NULL);
    while (pSubVol != NULL) {
        const char* subVolName;
        int ret;

        subVolName = pSubVol->GetDiskFS()->GetVolumeName();
        if (subVolName == NULL)
            subVolName = "+++";     // could probably do better than this

        ret = LoadDiskFSContents(pSubVol->GetDiskFS(), subVolName, pScanOpts);
        if (ret != 0)
            return ret;
        pSubVol = pDiskFS->GetNextSubVolume(pSubVol);
    }

    return 0;
}
