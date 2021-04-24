/*
 * CiderPress
 * Copyright (C) 2007, 2008 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Main window management.
 */
#include "stdafx.h"
#include "Main.h"
#include "MyApp.h"
#include "AboutDialog.h"
#include "NufxArchive.h"
#include "DiskArchive.h"
#include "BNYArchive.h"
#include "ACUArchive.h"
#include "AppleSingleArchive.h"
#include "ArchiveInfoDialog.h"
#include "PrefsDialog.h"
#include "EnterRegDialog.h"
#include "OpenVolumeDialog.h"
#include "Print.h"
#include "../util/UtilLib.h"
#include "resource.h"

/* use MFC's fancy version of new for debugging */
//#define new DEBUG_NEW

static const WCHAR kWebSiteURL[] = L"http://www.a2ciderpress.com/";

/* custom class name for main frame */
static const WCHAR kMainWindowClassName[] = L"faddenSoft.CiderPress.4";


/*
 * Filters for the "open file" command.  In some cases a file may be opened
 * in more than one format, so it's necessary to keep track of what the
 * file filter was set to when the file was opened.
 *
 * With Vista-style dialogs, the second part of the string (the filespec)
 * will sometimes be included in the pop-up.  Sometimes not.  It's
 * deterministic but I haven't been able to figure out what the pattern is --
 * it's not simply length of a given filter or of the entire string, or based
 * on the presence of certain characters.  The filter works correctly, so it
 * doesn't seem to be malformed.  It's just ugly to have the open dialog
 * popup show an enormous, redundant filter string.
 *
 * I tried substituting '\0' for '|' and placing the string directly into
 * the dialog; no change.
 *
 * CFileDialog::ApplyOFNToShellDialog in {VisualStudio}\VC\atlmfc\src\mfc\dlgfile.cpp
 * appears to be doing the parsing.  Single-stepping through the code shows
 * that it's working fine, so something later on is choosing to merge
 * pszName and pszSpec when generating the pop-up menu.
 *
 * The good news is that if I exclude the list of extensions from the name
 * section, the popup will (so far) always includes the spec.  The bad news
 * is that this means we won't display the list of extensions on WinXP, which
 * uses the older style of dialog.  We could switch from public constants to
 * a function that generates the filter based on a bit mask and the current
 * OS version, but that might be more trouble than it's worth.
 */
const WCHAR MainWindow::kOpenNuFX[] =
    L"ShrinkIt Archives|*.shk;*.sdk;*.bxy;*.sea;*.bse|";
const WCHAR MainWindow::kOpenBinaryII[] =
    L"Binary II Archives|*.bny;*.bqy;*.bxy|";
const WCHAR MainWindow::kOpenACU[] =
    L"ACU Archives|*.acu|";
const WCHAR MainWindow::kOpenAppleSingle[] =
    L"AppleSingle files|*.as|";
const WCHAR MainWindow::kOpenDiskImage[] =
    L"Disk Images|"
    L"*.shk;*.sdk;*.dsk;*.po;*.do;*.d13;*.2mg;*.img;*.nib;*.nb2;*.raw;*.hdv;*.dc;*.dc6;*.ddd;*.app;*.fdi;*.iso;*.gz;*.zip|";
const WCHAR MainWindow::kOpenAll[] =
    L"All Files|*.*|";
const WCHAR MainWindow::kOpenEnd[] =
    L"|";

/*
 * Used when guessing archive type from extension when no "-mode" argument
 * was specified.
 *
 * This does *not* apply to files double-clicked from the content list; see
 * HandleDoubleClick() for that.
 */
static const struct {
    WCHAR extension[4];
    FilterIndex idx;
} gExtensionToIndex[] = {
    { L"shk",   kFilterIndexDiskImage },    // DiskImage probably better than NuFX
    { L"bxy",   kFilterIndexNuFX },
    { L"bse",   kFilterIndexNuFX },
    { L"sea",   kFilterIndexNuFX },
    { L"bny",   kFilterIndexBinaryII },
    { L"bqy",   kFilterIndexBinaryII },
    { L"acu",   kFilterIndexACU },
    { L"as",    kFilterIndexAppleSingle },
    { L"sdk",   kFilterIndexDiskImage },
    { L"dsk",   kFilterIndexDiskImage },
    { L"po",    kFilterIndexDiskImage },
    { L"do",    kFilterIndexDiskImage },
    { L"d13",   kFilterIndexDiskImage },
    { L"2mg",   kFilterIndexDiskImage },
    { L"img",   kFilterIndexDiskImage },
    { L"nib",   kFilterIndexDiskImage },
    { L"nb2",   kFilterIndexDiskImage },
    { L"raw",   kFilterIndexDiskImage },
    { L"hdv",   kFilterIndexDiskImage },
    { L"dc",    kFilterIndexDiskImage },
    { L"dc6",   kFilterIndexDiskImage },
    { L"ddd",   kFilterIndexDiskImage },
    { L"app",   kFilterIndexDiskImage },
    { L"fdi",   kFilterIndexDiskImage },
    { L"iso",   kFilterIndexDiskImage },
    { L"gz",    kFilterIndexDiskImage },    // assume disk image inside
    { L"zip",   kFilterIndexDiskImage },    // assume disk image inside
};

const WCHAR MainWindow::kModeNuFX[] = L"nufx";
const WCHAR MainWindow::kModeBinaryII[] = L"bin2";
const WCHAR MainWindow::kModeACU[] = L"acu";
const WCHAR MainWindow::kModeAppleSingle[] = L"as";
const WCHAR MainWindow::kModeDiskImage[] = L"disk";


/*
 * ===========================================================================
 *      MainWindow
 * ===========================================================================
 */

static const UINT gFindReplaceID = RegisterWindowMessage(FINDMSGSTRING);

BEGIN_MESSAGE_MAP(MainWindow, CFrameWnd)
    ON_WM_CREATE()
    ON_MESSAGE(WMU_LATE_INIT, OnLateInit)
    //ON_MESSAGE(WMU_CLOSE_MAIN_DIALOG, OnCloseMainDialog)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_PAINT()
    //ON_WM_MOUSEWHEEL()
    ON_WM_SETFOCUS()
    ON_WM_HELPINFO()
    ON_WM_QUERYENDSESSION()
    ON_WM_ENDSESSION()
    ON_REGISTERED_MESSAGE(gFindReplaceID, OnFindDialogMessage)
    ON_COMMAND(          IDM_FILE_NEW_ARCHIVE, OnFileNewArchive)
    ON_COMMAND(          IDM_FILE_OPEN, OnFileOpen)
    ON_COMMAND(          IDM_FILE_OPEN_VOLUME, OnFileOpenVolume)
    ON_UPDATE_COMMAND_UI(IDM_FILE_OPEN_VOLUME, OnUpdateFileOpenVolume)
    ON_COMMAND(          IDM_FILE_REOPEN, OnFileReopen)
    ON_UPDATE_COMMAND_UI(IDM_FILE_REOPEN, OnUpdateFileReopen)
    ON_COMMAND(          IDM_FILE_SAVE, OnFileSave)
    ON_UPDATE_COMMAND_UI(IDM_FILE_SAVE, OnUpdateFileSave)
    ON_COMMAND(          IDM_FILE_CLOSE, OnFileClose)
    ON_UPDATE_COMMAND_UI(IDM_FILE_CLOSE, OnUpdateFileClose)
    ON_COMMAND(          IDM_FILE_ARCHIVEINFO, OnFileArchiveInfo)
    ON_UPDATE_COMMAND_UI(IDM_FILE_ARCHIVEINFO, OnUpdateFileArchiveInfo)
    ON_COMMAND(          IDM_FILE_PRINT, OnFilePrint)
    ON_UPDATE_COMMAND_UI(IDM_FILE_PRINT, OnUpdateFilePrint)
    ON_COMMAND(          IDM_FILE_EXIT, OnFileExit)
    ON_COMMAND(          IDM_EDIT_COPY, OnEditCopy)
    ON_UPDATE_COMMAND_UI(IDM_EDIT_COPY, OnUpdateEditCopy)
    ON_COMMAND(          IDM_EDIT_PASTE, OnEditPaste)
    ON_UPDATE_COMMAND_UI(IDM_EDIT_PASTE, OnUpdateEditPaste)
    ON_COMMAND(          IDM_EDIT_PASTE_SPECIAL, OnEditPasteSpecial)
    ON_UPDATE_COMMAND_UI(IDM_EDIT_PASTE_SPECIAL, OnUpdateEditPasteSpecial)
    ON_COMMAND(          IDM_EDIT_FIND, OnEditFind)
    ON_UPDATE_COMMAND_UI(IDM_EDIT_FIND, OnUpdateEditFind)
    ON_COMMAND(          IDM_EDIT_SELECT_ALL, OnEditSelectAll)
    ON_UPDATE_COMMAND_UI(IDM_EDIT_SELECT_ALL, OnUpdateEditSelectAll)
    ON_COMMAND(          IDM_EDIT_INVERT_SELECTION, OnEditInvertSelection)
    ON_UPDATE_COMMAND_UI(IDM_EDIT_INVERT_SELECTION, OnUpdateEditInvertSelection)
    ON_COMMAND(          IDM_EDIT_PREFERENCES, OnEditPreferences)
    ON_COMMAND_RANGE(    IDM_SORT_PATHNAME, IDM_SORT_ORIGINAL, OnEditSort)
    ON_UPDATE_COMMAND_UI_RANGE(IDM_SORT_PATHNAME, IDM_SORT_ORIGINAL, OnUpdateEditSort)
    ON_COMMAND(          IDM_ACTIONS_VIEW, OnActionsView)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_VIEW, OnUpdateActionsView)
    ON_COMMAND(          IDM_ACTIONS_ADD_FILES, OnActionsAddFiles)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_ADD_FILES, OnUpdateActionsAddFiles)
    ON_COMMAND(          IDM_ACTIONS_ADD_DISKS, OnActionsAddDisks)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_ADD_DISKS, OnUpdateActionsAddDisks)
    ON_COMMAND(          IDM_ACTIONS_CREATE_SUBDIR, OnActionsCreateSubdir)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_CREATE_SUBDIR, OnUpdateActionsCreateSubdir)
    ON_COMMAND(          IDM_ACTIONS_EXTRACT, OnActionsExtract)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_EXTRACT, OnUpdateActionsExtract)
    ON_COMMAND(          IDM_ACTIONS_TEST, OnActionsTest)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_TEST, OnUpdateActionsTest)
    ON_COMMAND(          IDM_ACTIONS_DELETE, OnActionsDelete)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_DELETE, OnUpdateActionsDelete)
    ON_COMMAND(          IDM_ACTIONS_RENAME, OnActionsRename)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_RENAME, OnUpdateActionsRename)
    ON_COMMAND(          IDM_ACTIONS_RECOMPRESS, OnActionsRecompress)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_RECOMPRESS, OnUpdateActionsRecompress)
    ON_COMMAND(          IDM_ACTIONS_OPENASDISK, OnActionsOpenAsDisk)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_OPENASDISK, OnUpdateActionsOpenAsDisk)
    ON_COMMAND(          IDM_ACTIONS_EDIT_COMMENT, OnActionsEditComment)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_EDIT_COMMENT, OnUpdateActionsEditComment)
    ON_COMMAND(          IDM_ACTIONS_EDIT_PROPS, OnActionsEditProps)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_EDIT_PROPS, OnUpdateActionsEditProps)
    ON_COMMAND(          IDM_ACTIONS_RENAME_VOLUME, OnActionsRenameVolume)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_RENAME_VOLUME, OnUpdateActionsRenameVolume)
    ON_COMMAND(          IDM_ACTIONS_CONV_DISK, OnActionsConvDisk)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_CONV_DISK, OnUpdateActionsConvDisk)
    ON_COMMAND(          IDM_ACTIONS_CONV_FILE, OnActionsConvFile)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_CONV_FILE, OnUpdateActionsConvFile)
    ON_COMMAND(          IDM_ACTIONS_CONV_TOWAV, OnActionsConvToWav)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_CONV_TOWAV, OnUpdateActionsConvToWav)
    ON_COMMAND(          IDM_ACTIONS_CONV_FROMWAV, OnActionsConvFromWav)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_CONV_FROMWAV, OnUpdateActionsConvFromWav)
    ON_COMMAND(          IDM_ACTIONS_IMPORT_BAS, OnActionsImportBAS)
    ON_UPDATE_COMMAND_UI(IDM_ACTIONS_IMPORT_BAS, OnUpdateActionsImportBAS)
    ON_COMMAND(          IDM_TOOLS_DISKEDIT, OnToolsDiskEdit)
    ON_COMMAND(          IDM_TOOLS_IMAGECREATOR, OnToolsDiskImageCreator)
    ON_COMMAND(          IDM_TOOLS_DISKCONV, OnToolsDiskConv)
    ON_COMMAND(          IDM_TOOLS_BULKDISKCONV, OnToolsBulkDiskConv)
    ON_COMMAND(          IDM_TOOLS_SST_MERGE, OnToolsSSTMerge)
    ON_COMMAND(          IDM_TOOLS_VOLUMECOPIER_VOLUME, OnToolsVolumeCopierVolume)
    ON_COMMAND(          IDM_TOOLS_VOLUMECOPIER_FILE, OnToolsVolumeCopierFile)
    ON_COMMAND(          IDM_TOOLS_EOLSCANNER, OnToolsEOLScanner)
    ON_COMMAND(          IDM_TOOLS_TWOIMGPROPS, OnToolsTwoImgProps)
    ON_COMMAND(          IDM_HELP_CONTENTS, OnHelpContents)
    ON_COMMAND(          IDM_HELP_WEBSITE, OnHelpWebSite)
    ON_COMMAND(          IDM_HELP_ORDERING, OnHelpOrdering)
    ON_COMMAND(          IDM_HELP_ABOUT, OnHelpAbout)
//  ON_COMMAND(          IDM_RTCLK_DEFAULT, OnRtClkDefault)

    ON_COMMAND(ID_HELP_FINDER, CFrameWnd::OnHelpFinder)
    ON_COMMAND(ID_HELP, CFrameWnd::OnHelp)
    ON_COMMAND(ID_CONTEXT_HELP, CFrameWnd::OnContextHelp)
    ON_COMMAND(ID_DEFAULT_HELP, CFrameWnd::OnHelpFinder)
END_MESSAGE_MAP()

/*
 * MainWindow constructor.  Creates the main window and sets
 * its properties.
 */
MainWindow::MainWindow()
{
    static const WCHAR kAppName[] = L"CiderPress";

    fpContentList = NULL;
    fpOpenArchive = NULL;
    //fpSelSet = NULL;
    fpActionProgress = NULL;
    fpProgressCounter = NULL;
    fpFindDialog = NULL;

    fFindDown = true;
    fFindMatchCase = false;
    fFindMatchWholeWord = false;

    fAbortPrinting = false;
    fhDevMode = NULL;
    fhDevNames = NULL;
    fNeedReopen = false;

    CString wndClass = AfxRegisterWndClass(
        CS_DBLCLKS /*| CS_HREDRAW | CS_VREDRAW*/,
        gMyApp.LoadStandardCursor(IDC_ARROW),
        NULL /*(HBRUSH) (COLOR_WINDOW + 1)*/,
        gMyApp.LoadIcon(IDR_MAINFRAME) );

    Create(wndClass, kAppName, WS_OVERLAPPEDWINDOW /*| WS_CLIPCHILDREN*/,
        rectDefault, NULL, MAKEINTRESOURCE(IDR_MAINFRAME));

    LoadAccelTable(MAKEINTRESOURCE(IDR_MAINFRAME));

    // initialize some OLE garbage
    AfxOleInit();

    // required by MFC if Rich Edit controls are used
    AfxInitRichEdit();

    // required??
    //AfxEnableControlContainer();

    SetCPTitle();

    int cc = PostMessage(WMU_LATE_INIT, 0, 0);
    ASSERT(cc != 0);
}

/*
 * MainWindow destructor.  Close the archive if one is open, but don't try
 * to shut down any controls in child windows.  By this point, Windows has
 * already snuffed them.
 */
MainWindow::~MainWindow()
{
    LOGI("~MainWindow");

    //LOGI("MainWindow destructor");
    CloseArchiveWOControls();

    //int cc;
    //cc = ::WinHelp(m_hWnd, ::AfxGetApp()->m_pszHelpFilePath, HELP_QUIT, 0);
    //LOGI("Turning off WinHelp returned %d", cc);
    ::HtmlHelp(NULL, NULL, HH_CLOSE_ALL, 0);

    // free stuff used by print dialog
    ::GlobalFree(fhDevMode);
    ::GlobalFree(fhDevNames);

    fPreferences.SaveToRegistry();
    LOGI("MainWindow destructor complete");
}

BOOL MainWindow::PreCreateWindow(CREATESTRUCT& cs)
{
    BOOL res = CFrameWnd::PreCreateWindow(cs);

    cs.dwExStyle &= ~(WS_EX_CLIENTEDGE);

    // This changes the window class name to a value that the installer can
    // detect.  This allows us to prevent installation while CiderPress is
    // running.  (If we don't do that, the installation will offer to reboot
    // the computer to complete installation.)
    WNDCLASS wndCls;
    if (!GetClassInfo(AfxGetInstanceHandle(), cs.lpszClass, &wndCls)) {
        LOGW("GetClassInfo failed");
    } else {
        cs.lpszClass = kMainWindowClassName;
        wndCls.lpszClassName = kMainWindowClassName;
        if (!AfxRegisterClass(&wndCls)) {
            LOGW("AfxRegisterClass failed");
        }
    }

    return res;
}

void MainWindow::GetClientRect(LPRECT lpRect) const
{
    CRect sizeRect;
    int toolBarHeight, statusBarHeight;

    fToolBar.GetWindowRect(&sizeRect);
    toolBarHeight = sizeRect.bottom - sizeRect.top;
    fStatusBar.GetWindowRect(&sizeRect);
    statusBarHeight = sizeRect.bottom - sizeRect.top;

    //LOGI("HEIGHTS = %d/%d", toolBarHeight, statusBarHeight);
    CFrameWnd::GetClientRect(lpRect);
    lpRect->top += toolBarHeight;
    lpRect->bottom -= statusBarHeight;
}

void MainWindow::DoIdle(void)
{
    /*
     * Make sure that the filename field in the content list is always
     * visible, since that what the user clicks on to select things.  Would
     * be nice to have a way to prevent it, but for now we'll just shove
     * things back where they're supposed to be.
     */
    if (fpContentList != NULL) {
        /* get the current column 0 width, with current user adjustments */
        fpContentList->ExportColumnWidths();
        int width = fPreferences.GetColumnLayout()->GetColumnWidth(0);

        if (width >= 0 && width < ColumnLayout::kMinCol0Width) {
            /* column is too small, but don't change it until user lets mouse up */
            if (::GetAsyncKeyState(VK_LBUTTON) >= 0) {
                LOGI("Resetting column 0 width");
                fPreferences.GetColumnLayout()->SetColumnWidth(0,
                    ColumnLayout::kMinCol0Width);
                fpContentList->NewColumnWidths();
            }
        }
    }

    /*
     * Put an asterisk at the end of the title if we have an open archive
     * and it has pending modifications.  Remove it if nothing is pending.
     */
    if (fpOpenArchive != NULL) {
        CString title;
        int len;

        GetWindowText(/*ref*/ title);
        len = title.GetLength();
        if (len > 0 && title.GetAt(len-1) == '*') {
            if (!fpOpenArchive->IsModified()) {
                /* remove the asterisk and the preceeding space */
                title.Delete(len-2, 2);
                SetWindowText(title);
            }
        } else {
            if (fpOpenArchive->IsModified()) {
                /* add an asterisk */
                title += " *";
                SetWindowText(title);
            }
        }
    }
}

void MainWindow::ProcessCommandLine(void)
{
    /*
     * Get the command line and break it down into an argument vector.
     *
     * Usage:
     *  CiderPress [[-temparc] [-mode {nufx,bin2,disk}] [-dispname name] filename]
     */
    const WCHAR* cmdLine = ::GetCommandLine();
    if (cmdLine == NULL || wcslen(cmdLine) == 0)
        return;

    WCHAR* mangle = wcsdup(cmdLine);
    if (mangle == NULL)
        return;

    LOGI("Mangling '%ls'", mangle);
    WCHAR* argv[8];
    int argc = 8;
    VectorizeString(mangle, argv, &argc);

    LOGI("Args:");
    for (int i = 0; i < argc; i++) {
        LOGI("  %d '%ls'", i, argv[i]);
    }

    /*
     * Figure out what the arguments are.
     */
    const WCHAR* filename = NULL;
    const WCHAR* dispName = NULL;
    FilterIndex filterIndex = kFilterIndexGeneric;
    bool temp = false;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (wcsicmp(argv[i], L"-mode") == 0) {
                if (i == argc-1) {
                    LOGI("WARNING: -mode specified without mode");
                } else
                    i++;
                if (wcsicmp(argv[i], kModeNuFX) == 0)
                    filterIndex = kFilterIndexNuFX;
                else if (wcsicmp(argv[i], kModeBinaryII) == 0)
                    filterIndex = kFilterIndexBinaryII;
                else if (wcsicmp(argv[i], kModeACU) == 0)
                    filterIndex = kFilterIndexACU;
                else if (wcsicmp(argv[i], kModeAppleSingle) == 0)
                    filterIndex = kFilterIndexAppleSingle;
                else if (wcsicmp(argv[i], kModeDiskImage) == 0)
                    filterIndex = kFilterIndexDiskImage;
                else {
                    LOGI("WARNING: unrecognized mode '%ls'", argv[i]);
                }
            } else if (wcsicmp(argv[i], L"-dispname") == 0) {
                if (i == argc-1) {
                    LOGI("WARNING: -dispname specified without name");
                } else
                    i++;
                dispName = argv[i];
            } else if (wcsicmp(argv[i], L"-temparc") == 0) {
                temp = true;
            } else if (wcsicmp(argv[i], L"-install") == 0) {
                // see MyApp::InitInstance
                LOGI("Got '-install' flag, doing nothing");
            } else if (wcsicmp(argv[i], L"-uninstall") == 0) {
                // see MyApp::InitInstance
                LOGI("Got '-uninstall' flag, doing nothing");
            } else {
                LOGI("WARNING: unrecognized flag '%ls'", argv[i]);
            }
        } else {
            /* must be the filename */
            if (i != argc-1) {
                LOGI("WARNING: ignoring extra arguments (e.g. '%ls')",
                    argv[i+1]);
            }
            filename = argv[i];
            break;
        }
    }
    if (argc != 1 && filename == NULL) {
        LOGI("WARNING: args specified but no filename found");
    }

    LOGI("Argument handling:");
    LOGI(" index=%d temp=%d filename='%ls'",
        filterIndex, temp, filename == NULL ? L"(null)" : filename);

    if (filename != NULL) {
        PathName path(filename);
        CString ext = path.GetExtension();

        // drop the leading '.' from the extension
        if (ext.Left(1) == ".")
            ext.Delete(0, 1);

        /* load the archive, mandating read-only if it's a temporary file */
        if (LoadArchive(filename, ext, filterIndex, temp) == 0) {
            /* success, update title bar */
            if (temp)
                fOpenArchivePathName = path.GetFileName();
            else
                fOpenArchivePathName = filename;
            if (dispName != NULL)
                fOpenArchivePathName = dispName;
            SetCPTitle(fOpenArchivePathName, fpOpenArchive);
        }

        /* if it's a temporary file, arrange to have it deleted before exit */
        if (temp) {
            int len = wcslen(filename);

            if (len > 4 && wcsicmp(filename + (len-4), L".tmp") == 0) {
                fDeleteList.Add(filename);
            } else {
                LOGI("NOT adding '%ls' to DeleteList -- does not end in '.tmp'",
                    filename);
            }
        }
    }

    free(mangle);
}


/*
 * ===================================
 *      Command handlers
 * ===================================
 */

const int kProgressPane = 1;

int MainWindow::OnCreate(LPCREATESTRUCT lpcs)
{
    // add a toolbar and status bar

    LOGI("Now in OnCreate!");
    if (CFrameWnd::OnCreate(lpcs) == -1)
        return -1;

    /*
     * Create the tool bar.
     */
#if 0
    static UINT buttonList[] = {
        IDM_FILE_OPEN,
        IDM_FILE_NEW_ARCHIVE,
        // spacer
        IDM_FILE_PRINT,
    };
#endif
    fToolBar.Create(this, WS_CHILD | WS_VISIBLE | CBRS_TOP |
        CBRS_TOOLTIPS | CBRS_FLYBY);
    fToolBar.LoadToolBar(IDR_TOOLBAR1);

    /*
     * Create the status bar.
     */
    static UINT indicators[] = { ID_SEPARATOR, ID_INDICATOR_COMPLETE };
    fStatusBar.Create(this);
    fStatusBar.SetIndicators(indicators, NELEM(indicators));
    //fStatusBar.SetPaneInfo(0, ID_SEPARATOR, SBPS_NOBORDERS | SBPS_STRETCH, 0);

    fStatusBar.SetPaneText(kProgressPane, L"");

    return 0;
}

LONG MainWindow::OnLateInit(UINT, LONG)
{
    /*
     * Catch a message sent to inspire us to perform one-time initializations of
     * preferences and libraries.
     *
     * We're doing this the long way around because we want to be able to
     * put up a dialog box if the version is bad.  If we tried to handle this
     * in the constructor we'd be acting before the window was fully created.
     */
    CString result;
    CString appName;
    CString niftyListFile;

    CheckedLoadString(&appName, IDS_MB_APP_NAME);

    LOGI("----- late init begins -----");

    /*
     * Handle all other messages.  This gives the framework a chance to dim
     * all of the toolbar buttons.  This is especially useful when opening
     * a file from the command line that doesn't exist, causing an error
     * dialog and blocking main window messages.
     */
    PeekAndPump();

    /*
     * Initialize libraries.  This includes a version check.
     */
    result = NufxArchive::AppInit();
    if (!result.IsEmpty())
        goto fail;
    result = DiskArchive::AppInit();
    if (!result.IsEmpty())
        goto fail;
    result = BnyArchive::AppInit();
    if (!result.IsEmpty())
        goto fail;

    niftyListFile = gMyApp.GetExeBaseName();
    niftyListFile += "NList.Data";
    if (!NiftyList::AppInit(niftyListFile)) {
        CString file2 = niftyListFile + ".TXT";
        if (!NiftyList::AppInit(file2)) {
            CString msg;
            msg.Format(IDS_NLIST_DATA_FAILED, niftyListFile, file2);
            MessageBox(msg, appName, MB_OK);
        }
    }

    /*
     * Read preferences from registry.
     */
    fPreferences.LoadFromRegistry();

#if 0
    /*
     * Check to see if we're registered; if we're not, and we've expired, it's
     * time to bail out.
     */
    MyRegistry::RegStatus regStatus;
    //regStatus = gMyApp.fRegistry.CheckRegistration(&result);
    regStatus = MyRegistry::kRegValid;
    LOGI("CheckRegistration returned %d", regStatus);
    switch (regStatus) {
    case MyRegistry::kRegNotSet:
    case MyRegistry::kRegValid:
        ASSERT(result.IsEmpty());
        break;
    case MyRegistry::kRegExpired:
    case MyRegistry::kRegInvalid:
        MessageBox(result, appName, MB_OK|MB_ICONINFORMATION);
        LOGI("FORCING REG");
        if (EnterRegDialog::GetRegInfo(this) != 0) {
            result = "";
            goto fail;
        }
        SetCPTitle();       // update title bar with new reg info
        break;
    case MyRegistry::kRegFailed:
        ASSERT(!result.IsEmpty());
        goto fail;
    default:
        ASSERT(false);
        CString confused;
        confused.Format(L"Registration check failed. %ls", (LPCWSTR) result);
        result = confused;
        goto fail;
    }
#endif

    /*
     * Process command-line options, possibly loading an archive.
     */
    ProcessCommandLine();

    return 0;

fail:
    if (!result.IsEmpty())
        ShowFailureMsg(this, result, IDS_FAILED);
    int cc = PostMessage(WM_CLOSE, 0, 0);
    ASSERT(cc != 0);

    return 0;
}

BOOL MainWindow::OnQueryEndSession(void)
{
    // The system wants to know if we're okay with shutting down.
    LOGI("Got QueryEndSession");
    return TRUE;
}

void MainWindow::OnEndSession(BOOL bEnding)
{
    LOGI("Got EndSession (bEnding=%d)", bEnding);

    if (bEnding) {
        CloseArchiveWOControls();

        fPreferences.SaveToRegistry();
    }
}

void MainWindow::OnSize(UINT nType, int cx, int cy)
{
    /*
     * The main window is resizing.  We don't automatically redraw on resize,
     * so we will need to update the client region.  If it's filled with a
     * control, the control's resize & redraw function will take care of it.
     * If not, we need to explicitly invalidate the client region so the
     * window will repaint itself.
     */
    CFrameWnd::OnSize(nType, cx, cy);
    ResizeClientArea();
}

void MainWindow::ResizeClientArea(void)
{
    CRect sizeRect;
    
    GetClientRect(&sizeRect);
    if (fpContentList != NULL)
        fpContentList->MoveWindow(sizeRect);
    else
        Invalidate(false);
}

void MainWindow::OnGetMinMaxInfo(MINMAXINFO* pMMI)
{
    // Restrict the minimum window size to something reasonable.
    pMMI->ptMinTrackSize.x = 256;
    pMMI->ptMinTrackSize.y = 192;
}

void MainWindow::OnPaint(void)
{
    // Repaint the main window.
    CPaintDC dc(this);
    CRect clientRect;

    GetClientRect(&clientRect);

    /*
     * If there's no control in the window, fill in the client area with
     * what looks like an empty MDI client rect.
     */
    if (fpContentList == NULL) {
        DrawEmptyClientArea(&dc, clientRect);
    }

#if 0
    CPen pen(PS_SOLID, 1, RGB(255, 0, 0));  // red pen, 1 pixel wide
    CPen* pOldPen = dc.SelectObject(&pen);

    dc.MoveTo(clientRect.left, clientRect.top);
    dc.LineTo(clientRect.right-1, clientRect.top);
    dc.LineTo(clientRect.right, clientRect.bottom);
    dc.LineTo(clientRect.left, clientRect.bottom-1);
    dc.LineTo(clientRect.left, clientRect.top);

    dc.SelectObject(pOldPen);
#endif
}

#if 0
afx_msg BOOL MainWindow::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    LOGI("MOUSE WHEEL");
    return FALSE;

    WPARAM wparam;
    LPARAM lparam;

    wparam = nFlags | (zDelta << 16);
    lparam = pt.x | (pt.y << 16);
    if (fpContentList != NULL)
        fpContentList->SendMessage(WM_MOUSEWHEEL, wparam, lparam);
    return CWnd::OnMouseWheel(nFlags, zDelta, pt);
//  return TRUE;
}
#endif

void MainWindow::OnSetFocus(CWnd* /*pOldWnd*/)
{
    // Make sure open controls keep the input focus.
    if (fpContentList != NULL) {
        LOGD("Returning focus to ContentList");
        fpContentList->SetFocus();
    }
}

BOOL MainWindow::OnHelpInfo(HELPINFO* /*lpHelpInfo*/)
{
    LOGD("MainWindow::OnHelpInfo");
    MyApp::HandleHelp(this, HELP_TOPIC_WELCOME);
    return TRUE;
}

void MainWindow::OnEditPreferences(void)
{
    // Handle Edit->Preferences by popping up a property sheet.

    PrefsSheet ps;
    ColumnLayout* pColLayout = fPreferences.GetColumnLayout();

    /* pull any user header tweaks out of list so we can configure prefs */
    if (fpContentList != NULL)
        fpContentList->ExportColumnWidths();

    /* set up PrefsGeneralPage */
    for (int i = 0; i < kNumVisibleColumns; i++) {
        ps.fGeneralPage.fColumn[i] = (pColLayout->GetColumnWidth(i) != 0);
    }
    ps.fGeneralPage.fMimicShrinkIt = fPreferences.GetPrefBool(kPrMimicShrinkIt);
    ps.fGeneralPage.fBadMacSHK = fPreferences.GetPrefBool(kPrBadMacSHK);
    ps.fGeneralPage.fReduceSHKErrorChecks = fPreferences.GetPrefBool(kPrReduceSHKErrorChecks);
    ps.fGeneralPage.fCoerceDOSFilenames = fPreferences.GetPrefBool(kPrCoerceDOSFilenames);
    ps.fGeneralPage.fSpacesToUnder = fPreferences.GetPrefBool(kPrSpacesToUnder);
    ps.fGeneralPage.fPasteJunkPaths = fPreferences.GetPrefBool(kPrPasteJunkPaths);
    ps.fGeneralPage.fBeepOnSuccess = fPreferences.GetPrefBool(kPrBeepOnSuccess);

    /* set up PrefsDiskImagePage */
    ps.fDiskImagePage.fQueryImageFormat = fPreferences.GetPrefBool(kPrQueryImageFormat);
    ps.fDiskImagePage.fOpenVolumeRO = fPreferences.GetPrefBool(kPrOpenVolumeRO);
    ps.fDiskImagePage.fOpenVolumePhys0 = fPreferences.GetPrefBool(kPrOpenVolumePhys0);
    ps.fDiskImagePage.fProDOSAllowLower = fPreferences.GetPrefBool(kPrProDOSAllowLower);
    ps.fDiskImagePage.fProDOSUseSparse = fPreferences.GetPrefBool(kPrProDOSUseSparse);

    /* set up PrefsCompressionPage */
    ps.fCompressionPage.fCompressType = fPreferences.GetPrefLong(kPrCompressionType);

    /* set up PrefsFviewPage */
    ps.fFviewPage.fMaxViewFileSizeKB =
        (fPreferences.GetPrefLong(kPrMaxViewFileSize) + 1023) / 1024;
    ps.fFviewPage.fNoWrapText = fPreferences.GetPrefBool(kPrNoWrapText);

    ps.fFviewPage.fHighlightHexDump = fPreferences.GetPrefBool(kPrHighlightHexDump);
    ps.fFviewPage.fHighlightBASIC = fPreferences.GetPrefBool(kPrHighlightBASIC);
    ps.fFviewPage.fConvDisasmOneByteBrkCop = fPreferences.GetPrefBool(kPrDisasmOneByteBrkCop);
    ps.fFviewPage.fConvMouseTextToASCII = fPreferences.GetPrefBool(kPrConvMouseTextToASCII);
    ps.fFviewPage.fConvHiResBlackWhite = fPreferences.GetPrefBool(kPrConvHiResBlackWhite);
    ps.fFviewPage.fConvDHRAlgorithm = fPreferences.GetPrefLong(kPrConvDHRAlgorithm);
    ps.fFviewPage.fRelaxGfxTypeCheck = fPreferences.GetPrefBool(kPrRelaxGfxTypeCheck);
//  ps.fFviewPage.fEOLConvRaw = fPreferences.GetPrefBool(kPrEOLConvRaw);
//  ps.fFviewPage.fConvHighASCII = fPreferences.GetPrefBool(kPrConvHighASCII);
    ps.fFviewPage.fConvTextEOL_HA = fPreferences.GetPrefBool(kPrConvTextEOL_HA);
    ps.fFviewPage.fConvCPMText = fPreferences.GetPrefBool(kPrConvCPMText);
    ps.fFviewPage.fConvPascalText = fPreferences.GetPrefBool(kPrConvPascalText);
    ps.fFviewPage.fConvPascalCode = fPreferences.GetPrefBool(kPrConvPascalCode);
    ps.fFviewPage.fConvApplesoft = fPreferences.GetPrefBool(kPrConvApplesoft);
    ps.fFviewPage.fConvInteger = fPreferences.GetPrefBool(kPrConvInteger);
    ps.fFviewPage.fConvBusiness = fPreferences.GetPrefBool(kPrConvBusiness);
    ps.fFviewPage.fConvGWP = fPreferences.GetPrefBool(kPrConvGWP);
    ps.fFviewPage.fConvText8 = fPreferences.GetPrefBool(kPrConvText8);
    ps.fFviewPage.fConvAWP = fPreferences.GetPrefBool(kPrConvAWP);
    ps.fFviewPage.fConvADB = fPreferences.GetPrefBool(kPrConvADB);
    ps.fFviewPage.fConvASP = fPreferences.GetPrefBool(kPrConvASP);
    ps.fFviewPage.fConvSCAssem = fPreferences.GetPrefBool(kPrConvSCAssem);
    ps.fFviewPage.fConvDisasm = fPreferences.GetPrefBool(kPrConvDisasm);
    ps.fFviewPage.fConvHiRes = fPreferences.GetPrefBool(kPrConvHiRes);
    ps.fFviewPage.fConvDHR = fPreferences.GetPrefBool(kPrConvDHR);
    ps.fFviewPage.fConvSHR = fPreferences.GetPrefBool(kPrConvSHR);
    ps.fFviewPage.fConvPrintShop = fPreferences.GetPrefBool(kPrConvPrintShop);
    ps.fFviewPage.fConvMacPaint = fPreferences.GetPrefBool(kPrConvMacPaint);
    ps.fFviewPage.fConvProDOSFolder = fPreferences.GetPrefBool(kPrConvProDOSFolder);
    ps.fFviewPage.fConvResources = fPreferences.GetPrefBool(kPrConvResources);

    /* set up PrefsFilesPage */
    ps.fFilesPage.fTempPath = fPreferences.GetPrefString(kPrTempPath);
    ps.fFilesPage.fExtViewerExts = fPreferences.GetPrefString(kPrExtViewerExts);

    if (ps.DoModal() == IDOK)
        ApplyNow(&ps);
}

void MainWindow::ApplyNow(PrefsSheet* pPS)
{
    // Apply a change from the preferences sheet.
    LOGV("APPLY CHANGES");

    bool mustReload = false;

    ColumnLayout* pColLayout = fPreferences.GetColumnLayout();

    if (pPS->fGeneralPage.fDefaultsPushed) {
        /* reset all sizes to defaults, then factor in checkboxes */
        LOGI(" Resetting all widths to defaults");

        /* copy defaults over */
        for (int i = 0; i < kNumVisibleColumns; i++)
            pColLayout->SetColumnWidth(i, ColumnLayout::kWidthDefaulted);
    }

    /* handle column checkboxes */
    for (int i = 0; i < kNumVisibleColumns; i++) {
        if (pColLayout->GetColumnWidth(i) == 0 &&
            pPS->fGeneralPage.fColumn[i])
        {
            /* restore column */
            LOGI(" Column %d restored", i);
            pColLayout->SetColumnWidth(i, ColumnLayout::kWidthDefaulted);
        } else if (pColLayout->GetColumnWidth(i) != 0 &&
            !pPS->fGeneralPage.fColumn[i])
        {
            /* disable column */
            LOGI(" Column %d hidden", i);
            pColLayout->SetColumnWidth(i, 0);
        }
    }
    if (fpContentList != NULL)
        fpContentList->NewColumnWidths();
    fPreferences.SetPrefBool(kPrMimicShrinkIt,
        pPS->fGeneralPage.fMimicShrinkIt != 0);
    fPreferences.SetPrefBool(kPrBadMacSHK, pPS->fGeneralPage.fBadMacSHK != 0);
    fPreferences.SetPrefBool(kPrReduceSHKErrorChecks,
        pPS->fGeneralPage.fReduceSHKErrorChecks != 0);
    if (fPreferences.GetPrefBool(kPrCoerceDOSFilenames)!=
        (pPS->fGeneralPage.fCoerceDOSFilenames != 0))
    {
        LOGI("DOS filename coercion pref now %d",
            pPS->fGeneralPage.fCoerceDOSFilenames);
        fPreferences.SetPrefBool(kPrCoerceDOSFilenames,
            pPS->fGeneralPage.fCoerceDOSFilenames != 0);
        mustReload = true;
    }
    if (fPreferences.GetPrefBool(kPrSpacesToUnder) !=
        (pPS->fGeneralPage.fSpacesToUnder != 0))
    {
        LOGI("Spaces-to-underscores now %d", pPS->fGeneralPage.fSpacesToUnder);
        fPreferences.SetPrefBool(kPrSpacesToUnder, pPS->fGeneralPage.fSpacesToUnder != 0);
        mustReload = true;
    }
    fPreferences.SetPrefBool(kPrPasteJunkPaths, pPS->fGeneralPage.fPasteJunkPaths != 0);
    fPreferences.SetPrefBool(kPrBeepOnSuccess, pPS->fGeneralPage.fBeepOnSuccess != 0);

    if (pPS->fGeneralPage.fOurAssociations != NULL) {
        LOGI("NEW ASSOCIATIONS!");

#ifdef CAN_UPDATE_FILE_ASSOC
        for (int assoc = 0; assoc < gMyApp.fRegistry.GetNumFileAssocs(); assoc++)
        {
            gMyApp.fRegistry.SetFileAssoc(assoc,
                pPS->fGeneralPage.fOurAssociations[assoc]);
        }
#endif

        /* delete them so, if they hit "apply" again, we only update once */
        delete[] pPS->fGeneralPage.fOurAssociations;
        pPS->fGeneralPage.fOurAssociations = NULL;

        LOGV("Sending association-change notification to Windows shell");
        ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }

    fPreferences.SetPrefBool(kPrQueryImageFormat, pPS->fDiskImagePage.fQueryImageFormat != 0);
    fPreferences.SetPrefBool(kPrOpenVolumeRO, pPS->fDiskImagePage.fOpenVolumeRO != 0);
    fPreferences.SetPrefBool(kPrOpenVolumePhys0, pPS->fDiskImagePage.fOpenVolumePhys0 != 0);
    fPreferences.SetPrefBool(kPrProDOSAllowLower, pPS->fDiskImagePage.fProDOSAllowLower != 0);
    fPreferences.SetPrefBool(kPrProDOSUseSparse, pPS->fDiskImagePage.fProDOSUseSparse != 0);

    fPreferences.SetPrefLong(kPrCompressionType, pPS->fCompressionPage.fCompressType);

    fPreferences.SetPrefLong(kPrMaxViewFileSize, pPS->fFviewPage.fMaxViewFileSizeKB * 1024);
    fPreferences.SetPrefBool(kPrNoWrapText, pPS->fFviewPage.fNoWrapText != 0);

    fPreferences.SetPrefBool(kPrHighlightHexDump, pPS->fFviewPage.fHighlightHexDump != 0);
    fPreferences.SetPrefBool(kPrHighlightBASIC, pPS->fFviewPage.fHighlightBASIC != 0);
    fPreferences.SetPrefBool(kPrDisasmOneByteBrkCop, pPS->fFviewPage.fConvDisasmOneByteBrkCop != 0);
    fPreferences.SetPrefBool(kPrConvMouseTextToASCII, pPS->fFviewPage.fConvMouseTextToASCII != 0);
    fPreferences.SetPrefBool(kPrConvHiResBlackWhite, pPS->fFviewPage.fConvHiResBlackWhite != 0);
    fPreferences.SetPrefLong(kPrConvDHRAlgorithm, pPS->fFviewPage.fConvDHRAlgorithm);
    fPreferences.SetPrefBool(kPrRelaxGfxTypeCheck, pPS->fFviewPage.fRelaxGfxTypeCheck != 0);
//  fPreferences.SetPrefBool(kPrEOLConvRaw, pPS->fFviewPage.fEOLConvRaw != 0);
//  fPreferences.SetPrefBool(kPrConvHighASCII, pPS->fFviewPage.fConvHighASCII != 0);
    fPreferences.SetPrefBool(kPrConvTextEOL_HA, pPS->fFviewPage.fConvTextEOL_HA != 0);
    fPreferences.SetPrefBool(kPrConvCPMText, pPS->fFviewPage.fConvCPMText != 0);
    fPreferences.SetPrefBool(kPrConvPascalText, pPS->fFviewPage.fConvPascalText != 0);
    fPreferences.SetPrefBool(kPrConvPascalCode, pPS->fFviewPage.fConvPascalCode != 0);
    fPreferences.SetPrefBool(kPrConvApplesoft, pPS->fFviewPage.fConvApplesoft != 0);
    fPreferences.SetPrefBool(kPrConvInteger, pPS->fFviewPage.fConvInteger != 0);
    fPreferences.SetPrefBool(kPrConvBusiness, pPS->fFviewPage.fConvBusiness != 0);
    fPreferences.SetPrefBool(kPrConvGWP, pPS->fFviewPage.fConvGWP != 0);
    fPreferences.SetPrefBool(kPrConvText8, pPS->fFviewPage.fConvText8 != 0);
    fPreferences.SetPrefBool(kPrConvAWP, pPS->fFviewPage.fConvAWP != 0);
    fPreferences.SetPrefBool(kPrConvADB, pPS->fFviewPage.fConvADB != 0);
    fPreferences.SetPrefBool(kPrConvASP, pPS->fFviewPage.fConvASP != 0);
    fPreferences.SetPrefBool(kPrConvSCAssem, pPS->fFviewPage.fConvSCAssem != 0);
    fPreferences.SetPrefBool(kPrConvDisasm, pPS->fFviewPage.fConvDisasm != 0);
    fPreferences.SetPrefBool(kPrConvHiRes, pPS->fFviewPage.fConvHiRes != 0);
    fPreferences.SetPrefBool(kPrConvDHR, pPS->fFviewPage.fConvDHR != 0);
    fPreferences.SetPrefBool(kPrConvSHR, pPS->fFviewPage.fConvSHR != 0);
    fPreferences.SetPrefBool(kPrConvPrintShop, pPS->fFviewPage.fConvPrintShop != 0);
    fPreferences.SetPrefBool(kPrConvMacPaint, pPS->fFviewPage.fConvMacPaint != 0);
    fPreferences.SetPrefBool(kPrConvProDOSFolder, pPS->fFviewPage.fConvProDOSFolder != 0);
    fPreferences.SetPrefBool(kPrConvResources, pPS->fFviewPage.fConvResources != 0);

    fPreferences.SetPrefString(kPrTempPath, pPS->fFilesPage.fTempPath);
    LOGI("--- Temp path now '%ls'", fPreferences.GetPrefString(kPrTempPath));
    fPreferences.SetPrefString(kPrExtViewerExts, pPS->fFilesPage.fExtViewerExts);


//  if ((pPS->fGeneralPage.fShowToolbarText != 0) != fPreferences.GetShowToolbarText()) {
//      fPreferences.SetShowToolbarText(pPS->fGeneralPage.fShowToolbarText != 0);
//      //SetToolbarTextMode();
//      ResizeClientArea();
//  }

    /* allow open archive to track changes to preferences */
    if (fpOpenArchive != NULL)
        fpOpenArchive->PreferencesChanged();

    if (mustReload) {
        LOGI("Preferences apply requesting GA/CL reload");
        if (fpOpenArchive != NULL)
            fpOpenArchive->Reload();
        if (fpContentList != NULL)
            fpContentList->Reload();
    }

    /* export to registry */
    fPreferences.SaveToRegistry();

    //Invalidate();
}

void MainWindow::OnEditFind(void)
{
    // Handle IDM_EDIT_FIND.

    DWORD flags = 0;

    if (fpFindDialog != NULL)
        return;

    if (fFindDown)
        flags |= FR_DOWN;
    if (fFindMatchCase)
        flags |= FR_MATCHCASE;
    if (fFindMatchWholeWord)
        flags |= FR_WHOLEWORD;

    fpFindDialog = new CFindReplaceDialog;

    fpFindDialog->Create(TRUE,      // "find" only
                         fFindLastStr,  // default string to search for
                         NULL,      // default string to replace
                         flags,     // flags
                         this);     // parent
}

void MainWindow::OnUpdateEditFind(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpOpenArchive != NULL);
}

LRESULT MainWindow::OnFindDialogMessage(WPARAM wParam, LPARAM lParam)
{
    // Handle activity in the modeless "find" dialog.

    assert(fpFindDialog != NULL);

    fFindDown = (fpFindDialog->SearchDown() != 0);
    fFindMatchCase = (fpFindDialog->MatchCase() != 0);
    fFindMatchWholeWord = (fpFindDialog->MatchWholeWord() != 0);

    if (fpFindDialog->IsTerminating()) {
        fpFindDialog = NULL;
        return 0;
    }

    if (fpFindDialog->FindNext()) {
        fFindLastStr = fpFindDialog->GetFindString();
        fpContentList->FindNext(fFindLastStr, fFindDown, fFindMatchCase,
            fFindMatchWholeWord);
    } else {
        LOGI("Unexpected find dialog activity");
    }

    return 0;
}

void MainWindow::OnEditSort(UINT id)
{
    // Handle IDM_SORT_*.
    // The "sort" menu item should really only be active if we have a file open.
    LOGD("EDIT SORT %d", id);

    ASSERT(id >= IDM_SORT_PATHNAME && id <= IDM_SORT_ORIGINAL);
    fPreferences.GetColumnLayout()->SetSortColumn(id - IDM_SORT_PATHNAME);
    fPreferences.GetColumnLayout()->SetAscending(true);
    if (fpContentList != NULL)
        fpContentList->NewSortOrder();
}

void MainWindow::OnUpdateEditSort(CCmdUI* pCmdUI)
{
    unsigned int column = fPreferences.GetColumnLayout()->GetSortColumn();

    pCmdUI->SetCheck(pCmdUI->m_nID - IDM_SORT_PATHNAME == column);
}

void MainWindow::OnHelpContents(void)
{
    MyApp::HandleHelp(this, HELP_TOPIC_WELCOME);
    //WinHelp(0, HELP_FINDER);
}

void MainWindow::OnHelpWebSite(void)
{
    int err;

    err = (int) ::ShellExecute(m_hWnd, L"open", kWebSiteURL, NULL, NULL,
                    SW_SHOWNORMAL);
    if (err <= 32) {
        CString msg;
        if (err == ERROR_FILE_NOT_FOUND) {
            msg = L"Windows call failed: web browser not found.  (Sometimes"
                  L" it mistakenly reports this when IE is not the default"
                  L" browser.)";
            ShowFailureMsg(this, msg, IDS_FAILED);
        } else {
            msg.Format(L"Unable to launch web browser (err=%d).", err);
            ShowFailureMsg(this, msg, IDS_FAILED);
        }
    }
}

void MainWindow::OnHelpOrdering(void)
{
    // How to order... ka-ching!
    // TODO: no longer used?
    LOGW("OnHelpOrdering -- not implemented");
    //WinHelp(HELP_TOPIC_ORDERING_INFO, HELP_CONTEXT);
}

void MainWindow::OnHelpAbout(void)
{
    int result;

    AboutDialog dlg(this);

    result = dlg.DoModal();
    LOGV("HelpAbout returned %d", result);

    /*
     * User could've changed registration.  If we're showing the registered
     * user name in the title bar, update it.
     */
    if (fpOpenArchive == NULL)
        SetCPTitle();
}

void MainWindow::OnFileNewArchive(void)
{
    // Create a new SHK archive, using a "save as" dialog to select the name.

    CString filename, saveFolder, errStr;
    GenericArchive* pOpenArchive;
    CString errMsg;

    CFileDialog dlg(FALSE, L"shk", NULL,
        OFN_OVERWRITEPROMPT|OFN_NOREADONLYRETURN|OFN_HIDEREADONLY,
        L"ShrinkIt Archives (*.shk)|*.shk||", this);

    dlg.m_ofn.lpstrTitle = L"New Archive";
    dlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenArchiveFolder);

    if (dlg.DoModal() != IDOK)
        goto bail;

    saveFolder = dlg.m_ofn.lpstrFile;
    saveFolder = saveFolder.Left(dlg.m_ofn.nFileOffset);
    fPreferences.SetPrefString(kPrOpenArchiveFolder, saveFolder);

    filename = dlg.GetPathName();
    LOGI("NEW FILE '%ls'", (LPCWSTR) filename);

    /* remove file if it already exists */
    errMsg = RemoveFile(filename);
    if (!errMsg.IsEmpty()) {
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    pOpenArchive = new NufxArchive;
    errStr = pOpenArchive->New(filename, NULL);
    if (!errStr.IsEmpty()) {
        CString failed;
        CheckedLoadString(&failed, IDS_FAILED);
        MessageBox(errStr, failed, MB_ICONERROR);

        delete pOpenArchive;
    } else {
        SwitchContentList(pOpenArchive);
        fOpenArchivePathName = dlg.GetPathName();
        SetCPTitle(fOpenArchivePathName, fpOpenArchive);
    }

bail:
    LOGD("--- OnFileNewArchive done");
}

void MainWindow::OnFileOpen(void)
{
    // Handle request to open an archive or disk image.

    CString openFilters;
    CString saveFolder;

    /* set up filters; the order must match enum FilterIndex */
    openFilters = kOpenNuFX;
    openFilters += kOpenBinaryII;
    openFilters += kOpenACU;
    openFilters += kOpenAppleSingle;
    openFilters += kOpenDiskImage;
    openFilters += kOpenAll;
    openFilters += kOpenEnd;
    CFileDialog dlg(TRUE, L"shk", NULL,
        OFN_FILEMUSTEXIST, openFilters, this);

    DWORD savedIndex = fPreferences.GetPrefLong(kPrLastOpenFilterIndex);
    if (savedIndex < kFilterIndexFIRST || savedIndex > kFilterIndexMAX) {
        // default to *.* if not set (zero) or out of range
        savedIndex = kFilterIndexGeneric;
    }
    dlg.m_ofn.nFilterIndex = savedIndex;
    dlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenArchiveFolder);

    if (dlg.DoModal() != IDOK)
        goto bail;

    fPreferences.SetPrefLong(kPrLastOpenFilterIndex, dlg.m_ofn.nFilterIndex);
    saveFolder = dlg.m_ofn.lpstrFile;
    saveFolder = saveFolder.Left(dlg.m_ofn.nFileOffset);
    fPreferences.SetPrefString(kPrOpenArchiveFolder, saveFolder);

    FilterIndex filterIndex = (FilterIndex) dlg.m_ofn.nFilterIndex;
    if (filterIndex < kFilterIndexFIRST || filterIndex > kFilterIndexMAX) {
        ASSERT(false);
        LOGW("invalid filterIndex %d", filterIndex);
        filterIndex = kFilterIndexGeneric;
    }
    DoOpenArchive(dlg.GetPathName(), dlg.GetFileExt(),
        filterIndex, dlg.GetReadOnlyPref() != 0);

bail:
    LOGD("--- OnFileOpen done");
}

void MainWindow::OnFileOpenVolume(void)
{
    // Handle request to open a raw disk volume.
    LOGD("--- OnFileOpenVolume");

    int result;

    OpenVolumeDialog dlg(this);

    result = dlg.DoModal();
    if (result != IDOK)
        goto bail;

    //DiskImg::SetAllowWritePhys0(fPreferences.GetPrefBool(kPrOpenVolumePhys0));
    DoOpenVolume(dlg.fChosenDrive, dlg.fReadOnly != 0);

bail:
    return;
}

void MainWindow::OnUpdateFileOpenVolume(CCmdUI* pCmdUI)
{
    // don't really need this function
    pCmdUI->Enable(TRUE);
}

void MainWindow::DoOpenArchive(const WCHAR* pathName, const WCHAR* ext,
    FilterIndex filterIndex, bool readOnly)
{
    if (LoadArchive(pathName, ext, filterIndex, readOnly) == 0) {
        /* success, update title bar */
        fOpenArchivePathName = pathName;
        SetCPTitle(fOpenArchivePathName, fpOpenArchive);
    } else {
        /* some failures will close an open archive */
        //if (fpOpenArchive == NULL)
        //  SetCPTitle();
    }
}

void MainWindow::OnFileReopen(void)
{
    ReopenArchive();
}

void MainWindow::OnUpdateFileReopen(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpOpenArchive != NULL);
}

void MainWindow::OnFileSave(void)
{
    /*
     * This may be called directly from tools, so don't assume that the
     * conditions checked for in OnUpdateFileSave hold here.
     */
    CString errMsg;

    if (fpOpenArchive == NULL)
        return;

    {
        CWaitCursor waitc;
        errMsg = fpOpenArchive->Flush();
    }
    if (!errMsg.IsEmpty())
        ShowFailureMsg(this, errMsg, IDS_FAILED);

    // update the title bar
    DoIdle();
}

void MainWindow::OnUpdateFileSave(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpOpenArchive != NULL && fpOpenArchive->IsModified());
}

void MainWindow::OnFileClose(void)
{
    CloseArchive();
    //SetCPTitle();
    LOGD("--- OnFileClose done");
}

void MainWindow::OnUpdateFileClose(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpOpenArchive != NULL);
}

void MainWindow::OnFileArchiveInfo(void)
{
    ArchiveInfoDialog* pDlg = NULL;
    ASSERT(fpOpenArchive != NULL);

    switch (fpOpenArchive->GetArchiveKind()) {
    case GenericArchive::kArchiveNuFX:
        pDlg = new NufxArchiveInfoDialog((NufxArchive*) fpOpenArchive, this);
        break;
    case GenericArchive::kArchiveDiskImage:
        pDlg = new DiskArchiveInfoDialog((DiskArchive*) fpOpenArchive, this);
        break;
    case GenericArchive::kArchiveBNY:
        pDlg = new BnyArchiveInfoDialog((BnyArchive*) fpOpenArchive, this);
        break;
    case GenericArchive::kArchiveACU:
        pDlg = new AcuArchiveInfoDialog((AcuArchive*) fpOpenArchive, this);
        break;
    case GenericArchive::kArchiveAppleSingle:
        pDlg = new AppleSingleArchiveInfoDialog((AppleSingleArchive*) fpOpenArchive, this);
        break;
    default:
        LOGW("Unexpected archive type %d", fpOpenArchive->GetArchiveKind());
        ASSERT(false);
        return;
    };

    pDlg->DoModal();

    delete pDlg;
}

void MainWindow::OnUpdateFileArchiveInfo(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL);
}

void MainWindow::OnFilePrint(void)
{
    PrintListing(fpContentList);
}

void MainWindow::OnUpdateFilePrint(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL && fpContentList->GetItemCount() > 0);
}

void MainWindow::PrintListing(const ContentList* pContentList)
{
    CPrintDialog dlg(FALSE);    // use CPrintDialogEx for Win2K? CPageSetUpDialog?
    PrintContentList pcl;
    CDC dc;
    int itemCount, numPages;

    itemCount = pContentList->GetItemCount();
    numPages = (itemCount + (pcl.GetLinesPerPage()-1)) / pcl.GetLinesPerPage();

    dlg.m_pd.nFromPage = dlg.m_pd.nMinPage = 1;
    dlg.m_pd.nToPage = dlg.m_pd.nMaxPage = numPages;

    dlg.m_pd.hDevMode = fhDevMode;
    dlg.m_pd.hDevNames = fhDevNames;
    dlg.m_pd.Flags |= PD_USEDEVMODECOPIESANDCOLLATE;
    dlg.m_pd.Flags &= ~(PD_NOPAGENUMS);
    if (dlg.DoModal() != IDOK)
        return;
    if (dc.Attach(dlg.GetPrinterDC()) != TRUE) {
        CString msg;
        CheckedLoadString(&msg, IDS_PRINTER_NOT_USABLE);
        ShowFailureMsg(this, msg, IDS_FAILED);
        return;
    }

    pcl.Setup(&dc, this);
    if (dlg.m_pd.Flags & PD_PAGENUMS)
        pcl.Print(pContentList, dlg.m_pd.nFromPage, dlg.m_pd.nToPage);
    else
        pcl.Print(pContentList);

    fhDevMode = dlg.m_pd.hDevMode;
    fhDevNames = dlg.m_pd.hDevNames;
}

void MainWindow::OnFileExit(void)
{
    // Handle Exit item by sending a close request.
    SendMessage(WM_CLOSE, 0, 0);
}

void MainWindow::OnEditSelectAll(void)
{
    ASSERT(fpContentList != NULL);
    fpContentList->SelectAll();
}

void MainWindow::OnUpdateEditSelectAll(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL);
}

void MainWindow::OnEditInvertSelection(void)
{
    ASSERT(fpContentList != NULL);
    fpContentList->InvertSelection();
}

void MainWindow::OnUpdateEditInvertSelection(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL);
}

GenericEntry* MainWindow::GetSelectedItem(ContentList* pContentList)
{
    if (pContentList->GetSelectedCount() != 1)
        return NULL;

    POSITION posn;
    posn = pContentList->GetFirstSelectedItemPosition();
    if (posn == NULL) {
        ASSERT(false);
        return NULL;
    }
    int num = pContentList->GetNextSelectedItem(/*ref*/ posn);
    GenericEntry* pEntry = (GenericEntry*) pContentList->GetItemData(num);
    if (pEntry == NULL) {
        LOGW(" Glitch: couldn't find entry %d", num);
        ASSERT(false);
    }

    return pEntry;
}

void MainWindow::HandleDoubleClick(void)
{
    const uint32_t kTypeDimg = 0x64496d67;
    const uint32_t kCreatorDcpy = 0x64437079;
    bool handled = false;

    ASSERT(fpContentList != NULL);
    if (fpContentList->GetSelectedCount() == 0) {
        /* nothing selected, they double-clicked outside first column */
        LOGI("Double-click but nothing selected");
        return;
    }
    if (fpContentList->GetSelectedCount() != 1) {
        /* multiple items, just bring up viewer */
        HandleView();
        return;
    }

    /*
     * Find the GenericEntry that corresponds to this item.
     */
    GenericEntry* pEntry = GetSelectedItem(fpContentList);
    if (pEntry == NULL)
        return;

    LOGI(" Double-click got '%ls'", (LPCWSTR) pEntry->GetPathNameUNI());
    const WCHAR* ext;
    long fileType, auxType;

    ext = PathName::FindExtension(pEntry->GetPathNameUNI(), pEntry->GetFssep());
    fileType = pEntry->GetFileType();
    auxType = pEntry->GetAuxType();

    /*  // unit tests for MatchSemicolonList
    MatchSemicolonList("gif; jpeg; jpg", "jpeg");
    MatchSemicolonList("gif; jpeg; jpg", "jpg");
    MatchSemicolonList("gif; jpeg; jpg", "gif");
    MatchSemicolonList("gif;jpeg;jpg", "gif;");
    MatchSemicolonList("gif; jpeg; jpg", "jpe");
    MatchSemicolonList("gif; jpeg; jpg", "jpegx");
    MatchSemicolonList("gif; jpeg; jpg", "jp");
    MatchSemicolonList("gif; jpeg; jpg", "jpgx");
    MatchSemicolonList("gif; jpeg; jpg", "if");
    MatchSemicolonList("gif; jpeg; jpg", "gifs");
    MatchSemicolonList("gif, jpeg; jpg", "jpeg");
    MatchSemicolonList("", "jpeg");
    MatchSemicolonList(";", "jpeg");
    MatchSemicolonList("gif, jpeg; jpg", "");
    */

    /*
     * Figure out what to do with it.
     */
    CString extViewerExts;
    extViewerExts = fPreferences.GetPrefString(kPrExtViewerExts);
    if (ext != NULL && MatchSemicolonList(extViewerExts, ext+1)) {
        LOGI(" Launching external viewer for '%ls'", ext);
        TmpExtractForExternal(pEntry);
        handled = true;
    } else if (pEntry->GetRecordKind() == GenericEntry::kRecordKindFile) {
        if ((ext != NULL && (
                wcsicmp(ext, L".shk") == 0 ||
                wcsicmp(ext, L".sdk") == 0 ||
                wcsicmp(ext, L".bxy") == 0)) ||
            (fileType == 0xe0 && auxType == 0x8002))
        {
            LOGI(" Guessing NuFX");
            TmpExtractAndOpen(pEntry, GenericEntry::kDataThread, kModeNuFX);
            handled = true;
        } else
        if ((ext != NULL && (
                wcsicmp(ext, L".bny") == 0 ||
                wcsicmp(ext, L".bqy") == 0)) ||
            (fileType == 0xe0 && auxType == 0x8000))
        {
            LOGI(" Guessing Binary II");
            TmpExtractAndOpen(pEntry, GenericEntry::kDataThread, kModeBinaryII);
            handled = true;
        } else
        if ((ext != NULL && (
                wcsicmp(ext, L".acu") == 0)) ||
            (fileType == 0xe0 && auxType == 0x8001))
        {
            LOGI(" Guessing ACU");
            TmpExtractAndOpen(pEntry, GenericEntry::kDataThread, kModeACU);
            handled = true;
        } else
        if ((ext != NULL && (
                wcsicmp(ext, L".as") == 0)) ||
            (fileType == 0xe0 && auxType == 0x0001))
        {
            LOGI(" Guessing AppleSingle");
            TmpExtractAndOpen(pEntry, GenericEntry::kDataThread, kModeAppleSingle);
            handled = true;
        } else
        if (fileType == kTypeDimg && auxType == kCreatorDcpy &&
            pEntry->GetDataForkLen() == 819284)
        {
            /* type is dImg, creator is dCpy, length is 800K + DC stuff */
            LOGI(" Looks like a DiskCopy disk image");
            TmpExtractAndOpen(pEntry, GenericEntry::kDataThread, kModeDiskImage);
            handled = true;
        }
    } else if (pEntry->GetRecordKind() == GenericEntry::kRecordKindForkedFile) {
        /*
         * On Mac HFS volumes these can have resource forks (which we ignore).
         * We could probably just generally ignore resource forks for all of
         * the above files, rather than pulling this one out separately, but
         * this is the only one that could reasonably have a resource fork.
         */
        if (fileType == kTypeDimg && auxType == kCreatorDcpy &&
            pEntry->GetDataForkLen() == 819284)
        {
            /* type is dImg, creator is dCpy, length is 800K + DC stuff */
            LOGI(" Looks like a (forked) DiskCopy disk image");
            TmpExtractAndOpen(pEntry, GenericEntry::kDataThread, kModeDiskImage);
            handled = true;
        }
    } else if (pEntry->GetRecordKind() == GenericEntry::kRecordKindDisk) {
        LOGI(" Opening archived disk image");
        TmpExtractAndOpen(pEntry, GenericEntry::kDiskImageThread, kModeDiskImage);
        handled = true;
    }

    if (!handled) {
        // standard viewer
        HandleView();
    }

    /* set "/t" temp flag and delete afterward, warning user (?) */
}

int MainWindow::TmpExtractAndOpen(GenericEntry* pEntry, int threadKind,
    const WCHAR* modeStr)
{
    CString dispName;
    bool mustDelete = false;

    /*
     * Get the name to display in the title bar.  Double quotes will
     * screw it up, so we have to replace them.  (We could escape them,
     * but then we'd also have to escape the escape char.)
     */
    dispName = pEntry->GetFileName();
    dispName.Replace('"', '_');

    WCHAR nameBuf[MAX_PATH];
    UINT unique;
    unique = GetTempFileName(fPreferences.GetPrefString(kPrTempPath),
                L"CPfile", 0, nameBuf);
    if (unique == 0) {
        DWORD dwerr = ::GetLastError();
        LOGI("GetTempFileName failed on '%ls' (err=%ld)",
            fPreferences.GetPrefString(kPrTempPath), dwerr);
        return dwerr;
    }
    mustDelete = true;

    /*
     * Open the temp file and extract the data into it.
     */
    CString errMsg;
    int result;
    FILE* fp;

    fp = _wfopen(nameBuf, L"wb");
    if (fp != NULL) {
        LOGD("Extracting to '%ls' (unique=%d)", nameBuf, unique);
        result = pEntry->ExtractThreadToFile(threadKind, fp,
                    GenericEntry::kConvertEOLOff, GenericEntry::kConvertHAOff,
                    &errMsg);
        fclose(fp);
        if (result == IDOK) {
            /* success */
            CString parameters;

            parameters.Format(L"-mode %ls -dispname \"%ls\" -temparc \"%ls\"",
                modeStr, (LPCWSTR) dispName, nameBuf);
            int err;

            err = (int) ::ShellExecute(m_hWnd, L"open",
                    gMyApp.GetExeFileName(), parameters, NULL,
                            SW_SHOWNORMAL);
            if (err <= 32) {
                CString msg;
                msg.Format(L"Unable to launch CiderPress (err=%d).", err);
                ShowFailureMsg(this, msg, IDS_FAILED);
            } else {
                /* during dev, "missing DLL" causes false-positive success */
                LOGI("Successfully launched CiderPress, mode=%ls", modeStr);
                mustDelete = false;     // up to newly-launched app
            }
        } else {
            ShowFailureMsg(this, errMsg, IDS_FAILED);
        }
    } else {
        CString msg;
        msg.Format(L"Unable to open temp file '%ls'.", nameBuf);
        ::ShowFailureMsg(this, msg, IDS_FAILED);
    }

    if (mustDelete) {
        LOGI("Deleting '%ls'", nameBuf);
        _wunlink(nameBuf);
    }

    return 0;
}

int MainWindow::TmpExtractForExternal(GenericEntry* pEntry)
{
    const WCHAR* ext;

    ext = PathName::FindExtension(pEntry->GetPathNameUNI(), pEntry->GetFssep());

    WCHAR nameBuf[MAX_PATH];
    UINT unique;
    unique = GetTempFileName(fPreferences.GetPrefString(kPrTempPath),
                L"CPfile", 0, nameBuf);
    if (unique == 0) {
        DWORD dwerr = ::GetLastError();
        LOGI("GetTempFileName failed on '%ls' (err=%ld)",
            fPreferences.GetPrefString(kPrTempPath), dwerr);
        return dwerr;
    }
    fDeleteList.Add(nameBuf);       // file is created by GetTempFileName

    wcscat(nameBuf, ext);

    /*
     * Open the temp file and extract the data into it.
     */
    CString errMsg;
    int result;
    FILE* fp;

    fp = _wfopen(nameBuf, L"wb");
    if (fp != NULL) {
        fDeleteList.Add(nameBuf);   // second file created by fopen
        LOGI("Extracting to '%ls' (unique=%d)", nameBuf, unique);
        result = pEntry->ExtractThreadToFile(GenericEntry::kDataThread, fp,
                    GenericEntry::kConvertEOLOff, GenericEntry::kConvertHAOff,
                    &errMsg);
        fclose(fp);
        if (result == IDOK) {
            /* success */
            int err;

            err = (int) ::ShellExecute(m_hWnd, L"open", nameBuf, NULL,
                            NULL, SW_SHOWNORMAL);
            if (err <= 32) {
                CString msg;
                msg.Format(L"Unable to launch external viewer (err=%d).", err);
                ShowFailureMsg(this, msg, IDS_FAILED);
            } else {
                LOGI("Successfully launched external viewer");
            }
        } else {
            ShowFailureMsg(this, errMsg, IDS_FAILED);
        }
    } else {
        CString msg;
        msg.Format(L"Unable to open temp file '%ls'.", nameBuf);
        ShowFailureMsg(this, msg, IDS_FAILED);
    }

    return 0;
}

#if 0
/*
 * Handle a "default action" selection from the right-click menu.  The
 * action only applies to the record that was clicked on, so we need to
 * retrieve that from the control.
 */
void MainWindow::OnRtClkDefault(void)
{
    int idx;

    ASSERT(fpContentList != NULL);

    idx = fpContentList->GetRightClickItem();
    ASSERT(idx != -1);
    LOGI("OnRtClkDefault %d", idx);

    fpContentList->ClearRightClickItem();
}
#endif


/*
 * ===================================
 *      Progress meter
 * ===================================
 */

/*
 * There are two different mechanisms for reporting progress: ActionProgress
 * dialogs (for adding/extracting files) and a small box in the lower
 * right-hand corner (for opening archives).  These functions will set
 * the progress in the active action progress dialog if it exists, or
 * will set the percentage in the window frame if not.
 */

void MainWindow::SetProgressBegin(void)
{
    if (fpActionProgress != NULL)
        fpActionProgress->SetProgress(0);
    else
        fStatusBar.SetPaneText(kProgressPane, L"--%");
    //LOGI("  Complete: BEGIN");

    /* redraw stuff with the changes */
    (void) PeekAndPump();
}

int MainWindow::SetProgressUpdate(int percent, const WCHAR* oldName,
    const WCHAR* newName)
{
    int status = IDOK;

    if (fpActionProgress != NULL) {
        status = fpActionProgress->SetProgress(percent);
        if (oldName != NULL)
            fpActionProgress->SetArcName(oldName);
        if (newName != NULL)
            fpActionProgress->SetFileName(newName);
    } else {
        WCHAR buf[8];
        wsprintf(buf, L"%d%%", percent);
        fStatusBar.SetPaneText(kProgressPane, buf);
        //LOGI("  Complete: %ls", buf);
    }

    if (!PeekAndPump()) {
        LOGI("SetProgressUpdate: shutdown?!");
    }

    //EventPause(10);       // DEBUG DEBUG
    return status;
}

void MainWindow::SetProgressEnd(void)
{
    if (fpActionProgress != NULL)
        fpActionProgress->SetProgress(100);
    else
        fStatusBar.SetPaneText(kProgressPane, L"");
//  EventPause(100);        // DEBUG DEBUG
    //LOGI("  Complete: END");
}

bool MainWindow::SetProgressCounter(const WCHAR* str, long val)
{
    /* if the main window is enabled, user could activate menus */
    ASSERT(!IsWindowEnabled());

    if (fpProgressCounter != NULL) {
        //LOGI("SetProgressCounter '%ls' %d", str, val);
        CString msg;

        if (str != NULL)
            fpProgressCounter->SetCounterFormat(str);
        fpProgressCounter->SetCount((int) val);
    } else {
        if (val < 0) {
            fStatusBar.SetPaneText(kProgressPane, L"");
        } else {
            CString tmpStr;
            tmpStr.Format(L"%ld", val);
            fStatusBar.SetPaneText(kProgressPane, tmpStr);
        }
    }

    if (!PeekAndPump()) {
        LOGI("SetProgressCounter: shutdown?!");
    }
    //EventPause(10);       // DEBUG DEBUG

    if (fpProgressCounter != NULL)
        return !fpProgressCounter->GetCancel();
    else
        return true;
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

void MainWindow::EventPause(int duration)
{
    int count = duration / 10;

    for (int i = 0; i < count; i++) {
        PeekAndPump();
        ::Sleep(10);
    }
}

/*static*/ BOOL CALLBACK MainWindow::PrintAbortProc(HDC hDC, int nCode)
{
    MainWindow* pMain = (MainWindow*)::AfxGetMainWnd();

    pMain->PeekAndPump();
    if (pMain->GetAbortPrinting()) {
        LOGI("PrintAbortProc returning FALSE (abort printing)");
        return FALSE;
    }
    LOGI("  PrintAbortProc returning TRUE (continue printing)");
    return TRUE;
}

/*
 * ===================================
 *      Support functions
 * ===================================
 */

void MainWindow::DrawEmptyClientArea(CDC* pDC, const CRect& clientRect)
{
    CBrush brush;
    brush.CreateSolidBrush(::GetSysColor(COLOR_APPWORKSPACE));  // dk gray
    CBrush* pOldBrush = pDC->SelectObject(&brush);
    pDC->FillRect(&clientRect, &brush);
    pDC->SelectObject(pOldBrush);

    CPen penWH(PS_SOLID, 1, ::GetSysColor(COLOR_3DHIGHLIGHT));  // white
    CPen penLG(PS_SOLID, 1, ::GetSysColor(COLOR_3DLIGHT));      // lt gray
    CPen penDG(PS_SOLID, 1, ::GetSysColor(COLOR_3DSHADOW));     // dk gray
    CPen penBL(PS_SOLID, 1, ::GetSysColor(COLOR_3DDKSHADOW));   // near-black
    CPen* pOldPen = pDC->SelectObject(&penWH);
    //pDC->SelectObject(&penWH);
    pDC->MoveTo(clientRect.right-1, clientRect.top);
    pDC->LineTo(clientRect.right-1, clientRect.bottom-1);
    pDC->LineTo(clientRect.left-1, clientRect.bottom-1);
    pDC->SelectObject(&penBL);
    pDC->MoveTo(clientRect.right-3, clientRect.top+1);
    pDC->LineTo(clientRect.left+1, clientRect.top+1);
    pDC->LineTo(clientRect.left+1, clientRect.bottom-2);
    pDC->SelectObject(&penLG);
    pDC->MoveTo(clientRect.right-2, clientRect.top+1);
    pDC->LineTo(clientRect.right-2, clientRect.bottom-2);
    pDC->LineTo(clientRect.left, clientRect.bottom-2);
    pDC->SelectObject(&penDG);
    pDC->MoveTo(clientRect.right-2, clientRect.top);
    pDC->LineTo(clientRect.left, clientRect.top);
    pDC->LineTo(clientRect.left, clientRect.bottom-1);

    pDC->SelectObject(pOldPen);
}

GenericArchive* MainWindow::CreateArchiveInstance(FilterIndex filterIndex) const
{
    GenericArchive* pOpenArchive = NULL;

    switch (filterIndex) {
    case kFilterIndexNuFX:          pOpenArchive = new NufxArchive;         break;
    case kFilterIndexBinaryII:      pOpenArchive = new BnyArchive;          break;
    case kFilterIndexACU:           pOpenArchive = new AcuArchive;          break;
    case kFilterIndexAppleSingle:   pOpenArchive = new AppleSingleArchive;  break;
    case kFilterIndexDiskImage:     pOpenArchive = new DiskArchive;         break;
    default:                        ASSERT(false);                          break;
    }

    return pOpenArchive;
}

int MainWindow::LoadArchive(const WCHAR* fileName, const WCHAR* extension,
    FilterIndex filterIndex, bool readOnly)
{
    static const WCHAR kFileArchive[] = L"This appears to be a file archive.";
    GenericArchive::OpenResult openResult;
    const FilterIndex origFilterIndex = filterIndex;
    GenericArchive* pOpenArchive = NULL;

    LOGI("LoadArchive: '%ls' ro=%d idx=%d", fileName, readOnly, filterIndex);

    /* close any existing archive to avoid weirdness from re-open */
    CloseArchive();

    /*
     * If they used the "All Files (*.*)" filter, try guess based
     * on the file extension.
     */
    if (filterIndex == kFilterIndexGeneric) {
        int i;

        for (i = 0; i < NELEM(gExtensionToIndex); i++) {
            if (wcsicmp(extension, gExtensionToIndex[i].extension) == 0) {
                filterIndex = gExtensionToIndex[i].idx;
                break;
            }
        }

        if (i == NELEM(gExtensionToIndex)) {
            // no match found, use "disk image" as initial guess
            filterIndex = kFilterIndexDiskImage;
        }
    }

    /*
     * Try to open the file according to the specified filter index.  If
     * it works, we're done.  Trying this first ensures that you can choose
     * to open, say, a .SDK file as either ShrinkIt or Disk Image.
     *
     * It's possible to cancel the file open if you have "confirm disk image
     * format set" and the file is a disk image.  In that case we want to
     * return with an error result, but without showing an error dialog.
     */
    CString firstErrStr;
    pOpenArchive = CreateArchiveInstance(filterIndex);
    LOGD("First try: %ls", (LPCWSTR) pOpenArchive->GetDescription());
    openResult = pOpenArchive->Open(fileName, readOnly, &firstErrStr);
    if (openResult == GenericArchive::kResultSuccess) {
        // success!
        SwitchContentList(pOpenArchive);
        return 0;
    } else if (openResult == GenericArchive::kResultFileArchive) {
        if (wcsicmp(extension, L"zip") == 0) {
            // we could probably just return in this case
            firstErrStr = L"Zip archives with multiple files are not supported";
        } else {
            firstErrStr = kFileArchive;
        }
    } else if (openResult == GenericArchive::kResultCancel) {
        LOGD("canceled");
        delete pOpenArchive;
        return -1;
    }
    delete pOpenArchive;

    /*
     * That didn't work.  Try the others.
     */
    for (int i = kFilterIndexFIRST; i <= kFilterIndexLASTNG; i++) {
        if (i == filterIndex) continue;

        pOpenArchive = CreateArchiveInstance((FilterIndex) i);
        LOGD("Now trying: %ls", (LPCWSTR) pOpenArchive->GetDescription());
        CString dummyErrStr;
        openResult = pOpenArchive->Open(fileName, readOnly, &dummyErrStr);
        if (openResult == GenericArchive::kResultSuccess) {
            // success!
            SwitchContentList(pOpenArchive);
            return 0;
        } else if (openResult == GenericArchive::kResultCancel) {
            LOGD("cancelled");
            delete pOpenArchive;
            return -1;
        } else if (i == kFilterIndexNuFX && firstErrStr == kFileArchive) {
            // For .shk files we first check to see if it's a disk image,
            // then we try by file.  If it's a damaged file archive, we
            // really want to present the file archive failure message, not
            // "that looks like a file archive".  So we tweak the result a
            // little here.
            //
            // This doesn't catch all the cases where the NufxLib error
            // message is more useful than the DiskImg error message, but
            // it's hard to accurately determine what the most-accurate
            // message is in some cases.
            firstErrStr = dummyErrStr;
        }
        delete pOpenArchive;
    }

    /*
     * Nothing worked.  Show the original error message so that it matches
     * up with the chosen filter index -- if they seemed to be trying to
     * open an AppleSingle, don't tell them we failed because the file isn't
     * a disk image.
     *
     * If they were using the Generic filter, they'll get the message from
     * the disk image library.  It might make more sense to just say "we
     * couldn't figure out what this was", but most of the time people are
     * trying to open disk images anyway.
     */
    if (firstErrStr.IsEmpty()) {
        // not expected; put up a generic message if it happens
        firstErrStr = L"Unable to determine what kind of file this is.";
    }
    ShowFailureMsg(this, firstErrStr, IDS_FAILED);
    return -1;
}

int MainWindow::DoOpenVolume(CString drive, bool readOnly)
{
    int result = -1;

    ASSERT(drive.GetLength() > 0);

    CString errStr;
    //char filename[4] = "_:\\";
    //filename[0] = driveLetter;

    LOGI("FileOpenVolume '%ls' %d", (LPCWSTR)drive, readOnly);

    /* close existing archive */
    CloseArchive();

    GenericArchive* pOpenArchive = NULL;
    pOpenArchive = new DiskArchive;
    {
        CWaitCursor waitc;
        GenericArchive::OpenResult openResult;

        openResult = pOpenArchive->Open(drive, readOnly, &errStr);
        if (openResult == GenericArchive::kResultCancel) {
            // this bubbles out of the format confirmation dialog
            goto bail;
        } else if (openResult != GenericArchive::kResultSuccess) {
            if (!errStr.IsEmpty())
                ShowFailureMsg(this, errStr, IDS_FAILED);
            goto bail;
        }
    }

    // success!
    SwitchContentList(pOpenArchive);
    pOpenArchive = NULL;
    fOpenArchivePathName = drive;
    result = 0;

    fOpenArchivePathName = drive;
    SetCPTitle(fOpenArchivePathName, fpOpenArchive);

bail:
    if (pOpenArchive != NULL) {
        ASSERT(result != 0);
        delete pOpenArchive;
    }
    return result;
}

void MainWindow::ReopenArchive(void)
{
    if (fpOpenArchive == NULL) {
        ASSERT(false);
        return;
    }

    /* clear the flag, regardless of success or failure */
    fNeedReopen = false;

    GenericArchive* pOpenArchive = NULL;
    CString pathName = fpOpenArchive->GetPathName();
    bool readOnly = fpOpenArchive->IsReadOnly();
    GenericArchive::ArchiveKind archiveKind = fpOpenArchive->GetArchiveKind();
    GenericArchive::OpenResult openResult;
    CString errStr;

    /* if the open fails we *don't* want to leave the previous content up */
    LOGI("Reopening '%ls' ro=%d kind=%d",
        (LPCWSTR) pathName, readOnly, archiveKind);
    CloseArchive();

    switch (archiveKind) {
    case GenericArchive::kArchiveDiskImage:
        pOpenArchive = new DiskArchive;
        break;
    case GenericArchive::kArchiveNuFX:
        pOpenArchive = new NufxArchive;
        break;
    case GenericArchive::kArchiveBNY:
        pOpenArchive = new BnyArchive;
        break;
    default:
        ASSERT(false);
        return;
    }

    openResult = pOpenArchive->Open(pathName, readOnly, &errStr);
    if (openResult == GenericArchive::kResultCancel) {
        // this bubbles out of the format confirmation dialog
        goto bail;
    } else if (openResult != GenericArchive::kResultSuccess) {
        if (!errStr.IsEmpty())
            ShowFailureMsg(this, errStr, IDS_FAILED);
        goto bail;
    }

    LOGI(" Reopen was successful");
    SwitchContentList(pOpenArchive);
    pOpenArchive = NULL;
    SetCPTitle(pathName, fpOpenArchive);

bail:
    delete pOpenArchive;
}

bool MainWindow::IsOpenPathName(const WCHAR* path)
{
    if (fpOpenArchive == NULL)
        return false;

    if (wcsicmp(path, fpOpenArchive->GetPathName()) == 0)
        return true;

    return false;
}

void MainWindow::SwitchContentList(GenericArchive* pOpenArchive)
{
    assert(pOpenArchive != NULL);

    /*
     * We've got an archive opened successfully.  If we already had one
     * open, shut it.  (This assumes that closing an archive is a simple
     * matter of closing files and freeing storage.  If we needed to do
     * something that might fail, like flush changes, we should've done
     * that before getting this far to avoid confusion.)
     */
    if (fpOpenArchive != NULL)
        CloseArchive();

    ASSERT(fpOpenArchive == NULL);
    ASSERT(fpContentList == NULL);

    /*
     * Without this we get an assertion failure in CImageList::Attach if we
     * call here from ReopenArchive.  I think Windows needs to do some
     * cleanup, though I don't understand how the reopen case differs from
     * the usual case.  Maybe there's more stuff pending in the "reopen"
     * case?  In any event, this seems to work, which is all you can hope
     * for from MFC.  It does, however, make the screen flash, which it
     * didn't do before.
     *
     * UPDATE: this tripped once while I was debugging, even with this.  The
     * PeekAndPump function does force the idle loop to run, so I'm not sure
     * why it failed, unless the debugger somehow affected the idle
     * processing.  Yuck.
     *
     * The screen flash bugged me so I took it back out.  And the assert
     * didn't hit.  I really, really love Windows.
     */
    //PeekAndPump();


    fpContentList = new ContentList(pOpenArchive,
                                    fPreferences.GetColumnLayout());
    
    CRect sizeRect;
    GetClientRect(&sizeRect);
    fpContentList->Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL,
        sizeRect, this, IDC_CONTENT_LIST);

    fpOpenArchive = pOpenArchive;
}

void MainWindow::CloseArchiveWOControls(void)
{
    if (fpOpenArchive != NULL) {
        //fpOpenArchive->Close();
        LOGI("Deleting OpenArchive");
        delete fpOpenArchive;
        fpOpenArchive = NULL;
    }
}

void MainWindow::CloseArchive(void)
{
    CWaitCursor waitc;  // closing large compressed archive can be slow

    // destroy the ContentList
    if (fpContentList != NULL) {
        LOGI("Destroying ContentList");
        fpContentList->DestroyWindow(); // auto-cleanup invokes "delete"
        fpContentList = NULL;
    }

    // destroy the GenericArchive
    CloseArchiveWOControls();

    // reset the title bar
    SetCPTitle();
}

void MainWindow::SetCPTitle(const WCHAR* pathname, GenericArchive* pOpenArchive)
{
    ASSERT(pathname != NULL);
    CString title;
    CString appName;

    CheckedLoadString(&appName, IDS_MB_APP_NAME);

    CString archiveDescription(pOpenArchive->GetDescription());
    title.Format(L"%ls - %ls (%ls)", (LPCWSTR) appName, pathname,
        (LPCWSTR) archiveDescription);

    if (fpOpenArchive->IsReadOnly()) {
        CString readOnly;
        CheckedLoadString(&readOnly, IDS_READONLY);
        title += L" ";
        title += readOnly;
    }

    SetWindowText(title);
}

void MainWindow::SetCPTitle(void)
{
    CString appName, regName, title;
    CString user, company, reg, versions, expire;

#if 0
    if (gMyApp.fRegistry.GetRegistration(&user, &company, &reg, &versions,
            &expire) == 0)
    {
        if (reg.IsEmpty()) {
            regName += _T("  (unregistered)");
        } else {
            regName += _T("  (registered to ");
            regName += user;
            regName += _T(")");
            // include company?
        }
    }
#endif

    CheckedLoadString(&appName, IDS_MB_APP_NAME);
    title = appName + regName;
    SetWindowText(title);
}

CString MainWindow::GetPrintTitle(void)
{
    CString title;

    if (fpOpenArchive == NULL) {
        ASSERT(false);
        return title;
    }

    CString appName;
    CheckedLoadString(&appName, IDS_MB_APP_NAME);

    CString archiveDescription(fpOpenArchive->GetDescription());
    title.Format(L"%ls - %ls (%ls)", (LPCWSTR) appName,
        (LPCWSTR) fOpenArchivePathName, (LPCWSTR) archiveDescription);

    return title;
}

void MainWindow::SuccessBeep(void)
{
    const Preferences* pPreferences = GET_PREFERENCES();

    if (pPreferences->GetPrefBool(kPrBeepOnSuccess)) {
        LOGI("<happy-beep>");
        ::MessageBeep(MB_OK);
    }
}

void MainWindow::FailureBeep(void)
{
    const Preferences* pPreferences = GET_PREFERENCES();

    if (pPreferences->GetPrefBool(kPrBeepOnSuccess)) {
        LOGI("<failure-beep>");
        ::MessageBeep(MB_ICONEXCLAMATION);  // maybe MB_ICONHAND?
    }
}

CString MainWindow::RemoveFile(const WCHAR* fileName)
{
    CString errMsg;

    int cc;
    cc = _wunlink(fileName);
    if (cc < 0 && errno != ENOENT) {
        int err = errno;
        LOGI("Failed removing file '%ls', errno=%d", fileName, err);
        errMsg.Format(L"Unable to remove '%ls': %hs.",
            fileName, strerror(err));
        if (err == EACCES)
            errMsg += L"\n\n(Make sure the file isn't open.)";
    }

    return errMsg;
}

/*static*/ void MainWindow::ConfigureReformatFromPreferences(ReformatHolder* pReformat)
{
    const Preferences* pPreferences = GET_PREFERENCES();

    pReformat->SetReformatAllowed(ReformatHolder::kReformatRaw, true);
    pReformat->SetReformatAllowed(ReformatHolder::kReformatHexDump, true);

    pReformat->SetReformatAllowed(ReformatHolder::kReformatTextEOL_HA,
        pPreferences->GetPrefBool(kPrConvTextEOL_HA));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatResourceFork,
        pPreferences->GetPrefBool(kPrConvResources));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatProDOSDirectory,
        pPreferences->GetPrefBool(kPrConvProDOSFolder));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatPascalText,
        pPreferences->GetPrefBool(kPrConvPascalText));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatPascalCode,
        pPreferences->GetPrefBool(kPrConvPascalCode));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatCPMText,
        pPreferences->GetPrefBool(kPrConvCPMText));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatApplesoft,
        pPreferences->GetPrefBool(kPrConvApplesoft));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatApplesoft_Hilite,
        pPreferences->GetPrefBool(kPrConvApplesoft));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatInteger,
        pPreferences->GetPrefBool(kPrConvInteger));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatInteger_Hilite,
        pPreferences->GetPrefBool(kPrConvInteger));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatBusiness,
        pPreferences->GetPrefBool(kPrConvBusiness));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatBusiness_Hilite,
        pPreferences->GetPrefBool(kPrConvBusiness));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatSCAssem,
        pPreferences->GetPrefBool(kPrConvSCAssem));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatMerlin,
        pPreferences->GetPrefBool(kPrConvSCAssem));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatLISA2,
        pPreferences->GetPrefBool(kPrConvSCAssem));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatLISA3,
        pPreferences->GetPrefBool(kPrConvSCAssem));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatLISA4,
        pPreferences->GetPrefBool(kPrConvSCAssem));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatMonitor8,
        pPreferences->GetPrefBool(kPrConvDisasm));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatDisasmMerlin8,
        pPreferences->GetPrefBool(kPrConvDisasm));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatMonitor16Long,
        pPreferences->GetPrefBool(kPrConvDisasm));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatMonitor16Short,
        pPreferences->GetPrefBool(kPrConvDisasm));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatDisasmOrcam16,
        pPreferences->GetPrefBool(kPrConvDisasm));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatAWGS_WP,
        pPreferences->GetPrefBool(kPrConvGWP));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatTeach,
        pPreferences->GetPrefBool(kPrConvGWP));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatGWP,
        pPreferences->GetPrefBool(kPrConvGWP));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatMagicWindow,
        pPreferences->GetPrefBool(kPrConvText8));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatGutenberg,
        pPreferences->GetPrefBool(kPrConvGutenberg));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatAWP,
        pPreferences->GetPrefBool(kPrConvAWP));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatAWP,
        pPreferences->GetPrefBool(kPrConvAWP));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatADB,
        pPreferences->GetPrefBool(kPrConvADB));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatASP,
        pPreferences->GetPrefBool(kPrConvASP));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatHiRes,
        pPreferences->GetPrefBool(kPrConvHiRes));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatHiRes_BW,
        pPreferences->GetPrefBool(kPrConvHiRes));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatDHR_Latched,
        pPreferences->GetPrefBool(kPrConvDHR));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatDHR_BW,
        pPreferences->GetPrefBool(kPrConvDHR));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatDHR_Plain140,
        pPreferences->GetPrefBool(kPrConvDHR));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatDHR_Window,
        pPreferences->GetPrefBool(kPrConvDHR));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatSHR_PIC,
        pPreferences->GetPrefBool(kPrConvSHR));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatSHR_JEQ,
        pPreferences->GetPrefBool(kPrConvSHR));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatSHR_Paintworks,
        pPreferences->GetPrefBool(kPrConvSHR));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatSHR_Packed,
        pPreferences->GetPrefBool(kPrConvSHR));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatSHR_APF,
        pPreferences->GetPrefBool(kPrConvSHR));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatSHR_3200,
        pPreferences->GetPrefBool(kPrConvSHR));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatSHR_3201,
        pPreferences->GetPrefBool(kPrConvSHR));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatSHR_DG256,
        pPreferences->GetPrefBool(kPrConvSHR));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatSHR_DG3200,
        pPreferences->GetPrefBool(kPrConvSHR));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatPrintShop,
        pPreferences->GetPrefBool(kPrConvPrintShop));
    pReformat->SetReformatAllowed(ReformatHolder::kReformatMacPaint,
        pPreferences->GetPrefBool(kPrConvMacPaint));

    pReformat->SetOption(ReformatHolder::kOptHiliteHexDump,
        pPreferences->GetPrefBool(kPrHighlightHexDump));
    pReformat->SetOption(ReformatHolder::kOptHiliteBASIC,
        pPreferences->GetPrefBool(kPrHighlightBASIC));
    pReformat->SetOption(ReformatHolder::kOptHiResBW,
        pPreferences->GetPrefBool(kPrConvHiResBlackWhite));
    pReformat->SetOption(ReformatHolder::kOptDHRAlgorithm,
        pPreferences->GetPrefLong(kPrConvDHRAlgorithm));
    pReformat->SetOption(ReformatHolder::kOptRelaxGfxTypeCheck,
        pPreferences->GetPrefBool(kPrRelaxGfxTypeCheck));
    pReformat->SetOption(ReformatHolder::kOptOneByteBrkCop,
        pPreferences->GetPrefBool(kPrDisasmOneByteBrkCop));
    pReformat->SetOption(ReformatHolder::kOptMouseTextToASCII,
        pPreferences->GetPrefBool(kPrConvMouseTextToASCII));
}

/*static*/ ReformatHolder::SourceFormat MainWindow::ReformatterSourceFormat(
    DiskImg::FSFormat format)
{
    /*
     * Gutenberg both UsesDOSFileStructure and is formatted with 
     * kFormatGutenberg, so check for the latter first.
     */
    if (format == DiskImg::kFormatGutenberg)
        return ReformatHolder::kSourceFormatGutenberg;
    else if (DiskImg::UsesDOSFileStructure(format))
        return ReformatHolder::kSourceFormatDOS;
    else if (format == DiskImg::kFormatCPM)
        return ReformatHolder::kSourceFormatCPM;
    else
        return ReformatHolder::kSourceFormatGeneric;
}
