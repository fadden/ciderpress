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
#include "MyApp.h"
#include "AboutDialog.h"
#include "NufxArchive.h"
#include "DiskArchive.h"
#include "BNYArchive.h"
#include "ACUArchive.h"
#include "ArchiveInfoDialog.h"
#include "PrefsDialog.h"
#include "EnterRegDialog.h"
#include "OpenVolumeDialog.h"
#include "Print.h"
#include "HelpTopics.h"
#include "../util/UtilLib.h"
#include "resource.h"

/* use MFC's fancy version of new for debugging */
//#define new DEBUG_NEW

static const char* kWebSiteURL = "http://www.faddensoft.com/";

/*
 * Filters for the "open file" command.  In some cases a file may be opened
 * in more than one format, so it's necessary to keep track of what the
 * file filter was set to when the file was opened.
 */
const char MainWindow::kOpenNuFX[] =
	"ShrinkIt Archives (.shk .sdk .bxy .sea .bse)|*.shk;*.sdk;*.bxy;*.sea;*.bse|";
const char MainWindow::kOpenBinaryII[] =
	"Binary II Archives (.bny .bqy .bxy)|*.bny;*.bqy;*.bxy|";
const char MainWindow::kOpenACU[] =
	"ACU Archives (.acu)|*.acu|";
const char MainWindow::kOpenDiskImage[] =
	"Disk Images (.shk .sdk .dsk .po .do .d13 .2mg .img .nib .nb2 .raw .hdv .dc .dc6 .ddd .app .fdi .iso .gz .zip)|"
		"*.shk;*.sdk;*.dsk;*.po;*.do;*.d13;*.2mg;*.img;*.nib;*.nb2;*.raw;*.hdv;*.dc;*.dc6;*.ddd;*.app;*.fdi;*.iso;*.gz;*.zip|";
const char MainWindow::kOpenAll[] =
	"All Files (*.*)|*.*|";
const char MainWindow::kOpenEnd[] =
	"|";

static const struct {
	//const char* extension;
	char extension[4];
	FilterIndex idx;
} gExtensionToIndex[] = {
	{ "shk",	kFilterIndexNuFX },
	{ "bxy",	kFilterIndexNuFX },
	{ "bse",	kFilterIndexNuFX },
	{ "sea",	kFilterIndexNuFX },
	{ "bny",	kFilterIndexBinaryII },
	{ "bqy",	kFilterIndexBinaryII },
	{ "acu",	kFilterIndexACU },
	{ "dsk",	kFilterIndexDiskImage },
	{ "po",		kFilterIndexDiskImage },
	{ "do",		kFilterIndexDiskImage },
	{ "d13",	kFilterIndexDiskImage },
	{ "2mg",	kFilterIndexDiskImage },
	{ "img",	kFilterIndexDiskImage },
	{ "sdk",	kFilterIndexDiskImage },
	{ "raw",	kFilterIndexDiskImage },
	{ "ddd",	kFilterIndexDiskImage },
	{ "app",	kFilterIndexDiskImage },
	{ "fdi",	kFilterIndexDiskImage },
	{ "iso",	kFilterIndexDiskImage },
	{ "gz",		kFilterIndexDiskImage },	// assume disk image inside
	{ "zip",	kFilterIndexDiskImage },	// assume disk image inside
};

const char* MainWindow::kModeNuFX = _T("nufx");
const char* MainWindow::kModeBinaryII = _T("bin2");
const char* MainWindow::kModeACU = _T("acu");
const char* MainWindow::kModeDiskImage = _T("disk");


/*
 * ===========================================================================
 *		MainWindow
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
//	ON_COMMAND(          IDM_RTCLK_DEFAULT, OnRtClkDefault)

	/* this is required to allow "Help" button to work in PropertySheets (!) */
//	ON_COMMAND(ID_HELP, OnHelp)
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
	static const char* kAppName = _T("CiderPress");

	fpContentList = nil;
	fpOpenArchive = nil;
	//fpSelSet = nil;
	fpActionProgress = nil;
	fpProgressCounter = nil;
	fpFindDialog = nil;

	fFindDown = true;
	fFindMatchCase = false;
	fFindMatchWholeWord = false;

	fAbortPrinting = false;
	fhDevMode = nil;
	fhDevNames = nil;
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
	WMSG0("~MainWindow\n");

	//WMSG0("MainWindow destructor\n");
	CloseArchiveWOControls();

	int cc;
	cc = ::WinHelp(m_hWnd, ::AfxGetApp()->m_pszHelpFilePath, HELP_QUIT, 0);
	WMSG1("Turning off WinHelp returned %d\n", cc);

	// free stuff used by print dialog
	::GlobalFree(fhDevMode);
	::GlobalFree(fhDevNames);

	fPreferences.SaveToRegistry();
	WMSG0("MainWindow destructor complete\n");
}


/*
 * Override the pre-create function to tweak the window style.
 */
BOOL
MainWindow::PreCreateWindow(CREATESTRUCT& cs)
{
	BOOL res = CFrameWnd::PreCreateWindow(cs);

	cs.dwExStyle &= ~(WS_EX_CLIENTEDGE);

	return res;
}

/*
 * Override GetClientRect so we can factor in the status and tool bars.
 */
void
MainWindow::GetClientRect(LPRECT lpRect) const
{
	CRect sizeRect;
	int toolBarHeight, statusBarHeight;

	fToolBar.GetWindowRect(&sizeRect);
	toolBarHeight = sizeRect.bottom - sizeRect.top;
	fStatusBar.GetWindowRect(&sizeRect);
	statusBarHeight = sizeRect.bottom - sizeRect.top;

	//WMSG2("HEIGHTS = %d/%d\n", toolBarHeight, statusBarHeight);
	CFrameWnd::GetClientRect(lpRect);
	lpRect->top += toolBarHeight;
	lpRect->bottom -= statusBarHeight;
}


/*
 * Do some idle processing.
 */
void
MainWindow::DoIdle(void)
{
	/*
	 * Make sure that the filename field in the content list is always
	 * visible, since that what the user clicks on to select things.  Would
	 * be nice to have a way to prevent it, but for now we'll just shove
	 * things back where they're supposed to be.
	 */
	if (fpContentList != nil) {
		/* get the current column 0 width, with current user adjustments */
		fpContentList->ExportColumnWidths();
		int width = fPreferences.GetColumnLayout()->GetColumnWidth(0);

		if (width >= 0 && width < ColumnLayout::kMinCol0Width) {
			/* column is too small, but don't change it until user lets mouse up */
			if (::GetAsyncKeyState(VK_LBUTTON) >= 0) {
				WMSG0("Resetting column 0 width\n");
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
	if (fpOpenArchive != nil) {
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


/*
 * Handle command-line arguments.
 *
 * Usage:
 *	CiderPress [[-temparc] [-mode {nufx,bin2,disk}] [-dispname name] filename]
 */
void
MainWindow::ProcessCommandLine(void)
{
	/*
	 * Get the command line and break it down into an argument vector.
	 */
	const char* cmdLine = ::GetCommandLine();
	if (cmdLine == nil || strlen(cmdLine) == 0)
		return;

	char* mangle = strdup(cmdLine);
	if (mangle == nil)
		return;

	WMSG1("Mangling '%s'\n", mangle);
	char* argv[8];
	int argc = 8;
	VectorizeString(mangle, argv, &argc);

	WMSG0("Args:\n");
	for (int i = 0; i < argc; i++) {
		WMSG2("  %d '%s'\n", i, argv[i]);
	}

	/*
	 * Figure out what the arguments are.
	 */
	const char* filename = nil;
	const char* dispName = nil;
	int filterIndex = kFilterIndexGeneric;
	bool temp = false;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (strcasecmp(argv[i], "-mode") == 0) {
				if (i == argc-1) {
					WMSG0("WARNING: -mode specified without mode\n");
				} else
					i++;
				if (strcasecmp(argv[i], kModeNuFX) == 0)
					filterIndex = kFilterIndexNuFX;
				else if (strcasecmp(argv[i], kModeBinaryII) == 0)
					filterIndex = kFilterIndexBinaryII;
				else if (strcasecmp(argv[i], kModeACU) == 0)
					filterIndex = kFilterIndexACU;
				else if (strcasecmp(argv[i], kModeDiskImage) == 0)
					filterIndex = kFilterIndexDiskImage;
				else {
					WMSG1("WARNING: unrecognized mode '%s'\n", argv[i]);
				}
			} else if (strcasecmp(argv[i], "-dispname") == 0) {
				if (i == argc-1) {
					WMSG0("WARNING: -dispname specified without name\n");
				} else
					i++;
				dispName = argv[i];
			} else if (strcasecmp(argv[i], "-temparc") == 0) {
				temp = true;
			} else if (strcasecmp(argv[i], "-install") == 0) {
				// see MyApp::InitInstance
				WMSG0("Got '-install' flag, doing nothing\n");
			} else if (strcasecmp(argv[i], "-uninstall") == 0) {
				// see MyApp::InitInstance
				WMSG0("Got '-uninstall' flag, doing nothing\n");
			} else {
				WMSG1("WARNING: unrecognized flag '%s'\n", argv[i]);
			}
		} else {
			/* must be the filename */
			if (i != argc-1) {
				WMSG1("WARNING: ignoring extra arguments (e.g. '%s')\n",
					argv[i+1]);
			}
			filename = argv[i];
			break;
		}
	}
	if (argc != 1 && filename == nil) {
		WMSG0("WARNING: args specified but no filename found\n");
	}

	WMSG0("Argument handling:\n");
	WMSG3(" index=%d temp=%d filename='%s'\n",
		filterIndex, temp, filename == nil ? "(nil)" : filename);

	if (filename != nil) {
		PathName path(filename);
		CString ext = path.GetExtension();

		// drop the leading '.' from the extension
		if (ext.Left(1) == ".")
			ext.Delete(0, 1);

		/* load the archive, mandating read-only if it's a temporary file */
		if (LoadArchive(filename, ext, filterIndex, temp, false) == 0) {
			/* success, update title bar */
			if (temp)
				fOpenArchivePathName = path.GetFileName();
			else
				fOpenArchivePathName = filename;
			if (dispName != nil)
				fOpenArchivePathName = dispName;
			SetCPTitle(fOpenArchivePathName, fpOpenArchive);
		}

		/* if it's a temporary file, arrange to have it deleted before exit */
		if (temp) {
			int len = strlen(filename);

			if (len > 4 && strcasecmp(filename + (len-4), ".tmp") == 0) {
				fDeleteList.Add(filename);
			} else {
				WMSG1("NOT adding '%s' to DeleteList -- does not end in '.tmp'\n",
					filename);
			}
		}
	}

	free(mangle);
}


/*
 * ===================================
 *		Command handlers
 * ===================================
 */

const int kProgressPane = 1;

/*
 * OnCreate handler.  Used to add a toolbar and status bar.
 */
int
MainWindow::OnCreate(LPCREATESTRUCT lpcs)
{
	WMSG0("Now in OnCreate!\n");
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

	fStatusBar.SetPaneText(kProgressPane, "");

	return 0;
}


/*
 * Catch a message sent to inspire us to perform one-time initializations of
 * preferences and libraries.
 *
 * We're doing this the long way around because we want to be able to
 * put up a dialog box if the version is bad.  If we tried to handle this
 * in the constructor we'd be acting before the window was fully created.
 */
LONG
MainWindow::OnLateInit(UINT, LONG)
{
	CString result;
	CString appName;
	CString niftyListFile;

	appName.LoadString(IDS_MB_APP_NAME);

	WMSG0("----- late init begins -----\n");

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

	/*
	 * Check to see if we're registered; if we're not, and we've expired, it's
	 * time to bail out.
	 */
	MyRegistry::RegStatus regStatus;
	//regStatus = gMyApp.fRegistry.CheckRegistration(&result);
	regStatus = MyRegistry::kRegValid;
	WMSG1("CheckRegistration returned %d\n", regStatus);
	switch (regStatus) {
	case MyRegistry::kRegNotSet:
	case MyRegistry::kRegValid:
		ASSERT(result.IsEmpty());
		break;
	case MyRegistry::kRegExpired:
	case MyRegistry::kRegInvalid:
		MessageBox(result, appName, MB_OK|MB_ICONINFORMATION);
		WMSG0("FORCING REG\n");
#if 0
		if (EnterRegDialog::GetRegInfo(this) != 0) {
			result = "";
			goto fail;
		}
#endif
		SetCPTitle();		// update title bar with new reg info
		break;
	case MyRegistry::kRegFailed:
		ASSERT(!result.IsEmpty());
		goto fail;
	default:
		ASSERT(false);
		CString confused;
		confused.Format("Registration check failed. %s", (LPCTSTR) result);
		result = confused;
		goto fail;
	}

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


/*
 * The system wants to know if we're okay with shutting down.
 *
 * Return TRUE if it's okay to shut down, FALSE otherwise.
 */
BOOL
MainWindow::OnQueryEndSession(void)
{
	WMSG0("Got QueryEndSession\n");
	return TRUE;
}

/*
 * Notification of shutdown (or not).
 */
void
MainWindow::OnEndSession(BOOL bEnding)
{
	WMSG1("Got EndSession (bEnding=%d)\n", bEnding);

	if (bEnding) {
		CloseArchiveWOControls();

		fPreferences.SaveToRegistry();
	}
}

/*
 * The main window is resizing.  We don't automatically redraw on resize,
 * so we will need to update the client region.  If it's filled with a
 * control, the control's resize & redraw function will take care of it.
 * If not, we need to explicitly invalidate the client region so the
 * window will repaint itself.
 */
void
MainWindow::OnSize(UINT nType, int cx, int cy)
{
	CFrameWnd::OnSize(nType, cx, cy);
	ResizeClientArea();
}
void
MainWindow::ResizeClientArea(void)
{
	CRect sizeRect;
	
	GetClientRect(&sizeRect);
	if (fpContentList != NULL)
		fpContentList->MoveWindow(sizeRect);
	else
		Invalidate(false);
}

/*
 * Restrict the minimum window size to something reasonable.
 */
void
MainWindow::OnGetMinMaxInfo(MINMAXINFO* pMMI)
{
	pMMI->ptMinTrackSize.x = 256;
	pMMI->ptMinTrackSize.y = 192;
}

/*
 * Repaint the main window.
 */
void
MainWindow::OnPaint(void)
{
	CPaintDC dc(this);
	CRect clientRect;

	GetClientRect(&clientRect);

	/*
	 * If there's no control in the window, fill in the client area with
	 * what looks like an empty MDI client rect.
	 */
	if (fpContentList == nil) {
		DrawEmptyClientArea(&dc, clientRect);
	}

#if 0
	CPen pen(PS_SOLID, 1, RGB(255, 0, 0));	// red pen, 1 pixel wide
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
afx_msg BOOL
MainWindow::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	WMSG0("MOUSE WHEEL\n");
	return FALSE;

	WPARAM wparam;
	LPARAM lparam;

	wparam = nFlags | (zDelta << 16);
	lparam = pt.x | (pt.y << 16);
	if (fpContentList != nil)
		fpContentList->SendMessage(WM_MOUSEWHEEL, wparam, lparam);
	return CWnd::OnMouseWheel(nFlags, zDelta, pt);
//	return TRUE;
}
#endif

/*
 * Make sure open controls keep the input focus.
 */
void
MainWindow::OnSetFocus(CWnd* /*pOldWnd*/)
{
	if (fpContentList != nil) {
		WMSG0("Returning focus to ContentList\n");
		fpContentList->SetFocus();
	}
}

/*
 * User hit F1.  We don't currently have context-sensitive help on the main page.
 */
BOOL
MainWindow::OnHelpInfo(HELPINFO* /*lpHelpInfo*/)
{
	//WinHelp(0, HELP_FINDER);
	WinHelp(HELP_TOPIC_WELCOME, HELP_CONTEXT);
	return TRUE;	// dunno what this means
}

#if 0
/*
 * Catch-all Help handler, necessary to allow CPropertySheet to display a
 * "Help" button.  (WTF?)
 */
LONG
MainWindow::OnHelp(UINT wParam, LONG lParam)
{
	HELPINFO* lpHelpInfo = (HELPINFO*) lParam;

	DWORD context = lpHelpInfo->iCtrlId;
	WMSG1("MainWindow OnHelp (context=%d)\n", context);
	WinHelp(context, HELP_CONTEXTPOPUP);

	return TRUE;	// yes, we handled it
}
#endif

/*
 * Handle Edit->Preferences by popping up a property sheet.
 */
void
MainWindow::OnEditPreferences(void)
{
	PrefsSheet ps;
	ColumnLayout* pColLayout = fPreferences.GetColumnLayout();

	/* pull any user header tweaks out of list so we can configure prefs */
	if (fpContentList != nil)
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
	ps.fFviewPage.fConvHiResBlackWhite = fPreferences.GetPrefBool(kPrConvHiResBlackWhite);
	ps.fFviewPage.fConvDHRAlgorithm = fPreferences.GetPrefLong(kPrConvDHRAlgorithm);
	ps.fFviewPage.fRelaxGfxTypeCheck = fPreferences.GetPrefBool(kPrRelaxGfxTypeCheck);
//	ps.fFviewPage.fEOLConvRaw = fPreferences.GetPrefBool(kPrEOLConvRaw);
//	ps.fFviewPage.fConvHighASCII = fPreferences.GetPrefBool(kPrConvHighASCII);
	ps.fFviewPage.fConvTextEOL_HA = fPreferences.GetPrefBool(kPrConvTextEOL_HA);
	ps.fFviewPage.fConvCPMText = fPreferences.GetPrefBool(kPrConvCPMText);
	ps.fFviewPage.fConvPascalText = fPreferences.GetPrefBool(kPrConvPascalText);
	ps.fFviewPage.fConvPascalCode = fPreferences.GetPrefBool(kPrConvPascalCode);
	ps.fFviewPage.fConvApplesoft = fPreferences.GetPrefBool(kPrConvApplesoft);
	ps.fFviewPage.fConvInteger = fPreferences.GetPrefBool(kPrConvInteger);
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

/*
 * Apply a change from the preferences sheet.
 */
void
MainWindow::ApplyNow(PrefsSheet* pPS)
{
	bool mustReload = false;

	//WMSG0("APPLY CHANGES\n");

	ColumnLayout* pColLayout = fPreferences.GetColumnLayout();

	if (pPS->fGeneralPage.fDefaultsPushed) {
		/* reset all sizes to defaults, then factor in checkboxes */
		WMSG0(" Resetting all widths to defaults\n");

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
			WMSG1(" Column %d restored\n", i);
			pColLayout->SetColumnWidth(i, ColumnLayout::kWidthDefaulted);
		} else if (pColLayout->GetColumnWidth(i) != 0 &&
			!pPS->fGeneralPage.fColumn[i])
		{
			/* disable column */
			WMSG1(" Column %d hidden\n", i);
			pColLayout->SetColumnWidth(i, 0);
		}
	}
	if (fpContentList != nil)
		fpContentList->NewColumnWidths();
	fPreferences.SetPrefBool(kPrMimicShrinkIt,
		pPS->fGeneralPage.fMimicShrinkIt != 0);
	fPreferences.SetPrefBool(kPrBadMacSHK, pPS->fGeneralPage.fBadMacSHK != 0);
	fPreferences.SetPrefBool(kPrReduceSHKErrorChecks,
		pPS->fGeneralPage.fReduceSHKErrorChecks != 0);
	if (fPreferences.GetPrefBool(kPrCoerceDOSFilenames)!=
		(pPS->fGeneralPage.fCoerceDOSFilenames != 0))
	{
		WMSG1("DOS filename coercion pref now %d\n",
			pPS->fGeneralPage.fCoerceDOSFilenames);
		fPreferences.SetPrefBool(kPrCoerceDOSFilenames,
			pPS->fGeneralPage.fCoerceDOSFilenames != 0);
		mustReload = true;
	}
	if (fPreferences.GetPrefBool(kPrSpacesToUnder) !=
		(pPS->fGeneralPage.fSpacesToUnder != 0))
	{
		WMSG1("Spaces-to-underscores now %d\n", pPS->fGeneralPage.fSpacesToUnder);
		fPreferences.SetPrefBool(kPrSpacesToUnder, pPS->fGeneralPage.fSpacesToUnder != 0);
		mustReload = true;
	}
	fPreferences.SetPrefBool(kPrPasteJunkPaths, pPS->fGeneralPage.fPasteJunkPaths != 0);
	fPreferences.SetPrefBool(kPrBeepOnSuccess, pPS->fGeneralPage.fBeepOnSuccess != 0);

	if (pPS->fGeneralPage.fOurAssociations != nil) {
		WMSG0("NEW ASSOCIATIONS!\n");

		for (int assoc = 0; assoc < gMyApp.fRegistry.GetNumFileAssocs(); assoc++)
		{
			gMyApp.fRegistry.SetFileAssoc(assoc,
				pPS->fGeneralPage.fOurAssociations[assoc]);
		}

		/* delete them so, if they hit "apply" again, we only update once */
		delete[] pPS->fGeneralPage.fOurAssociations;
		pPS->fGeneralPage.fOurAssociations = nil;
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
	fPreferences.SetPrefBool(kPrConvHiResBlackWhite, pPS->fFviewPage.fConvHiResBlackWhite != 0);
	fPreferences.SetPrefLong(kPrConvDHRAlgorithm, pPS->fFviewPage.fConvDHRAlgorithm);
	fPreferences.SetPrefBool(kPrRelaxGfxTypeCheck, pPS->fFviewPage.fRelaxGfxTypeCheck != 0);
//	fPreferences.SetPrefBool(kPrEOLConvRaw, pPS->fFviewPage.fEOLConvRaw != 0);
//	fPreferences.SetPrefBool(kPrConvHighASCII, pPS->fFviewPage.fConvHighASCII != 0);
	fPreferences.SetPrefBool(kPrConvTextEOL_HA, pPS->fFviewPage.fConvTextEOL_HA != 0);
	fPreferences.SetPrefBool(kPrConvCPMText, pPS->fFviewPage.fConvCPMText != 0);
	fPreferences.SetPrefBool(kPrConvPascalText, pPS->fFviewPage.fConvPascalText != 0);
	fPreferences.SetPrefBool(kPrConvPascalCode, pPS->fFviewPage.fConvPascalCode != 0);
	fPreferences.SetPrefBool(kPrConvApplesoft, pPS->fFviewPage.fConvApplesoft != 0);
	fPreferences.SetPrefBool(kPrConvInteger, pPS->fFviewPage.fConvInteger != 0);
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
	WMSG1("--- Temp path now '%s'\n", fPreferences.GetPrefString(kPrTempPath));
	fPreferences.SetPrefString(kPrExtViewerExts, pPS->fFilesPage.fExtViewerExts);


//	if ((pPS->fGeneralPage.fShowToolbarText != 0) != fPreferences.GetShowToolbarText()) {
//		fPreferences.SetShowToolbarText(pPS->fGeneralPage.fShowToolbarText != 0);
//		//SetToolbarTextMode();
//		ResizeClientArea();
//	}

	/* allow open archive to track changes to preferences */
	if (fpOpenArchive != nil)
		fpOpenArchive->PreferencesChanged();

	if (mustReload) {
		WMSG0("Preferences apply requesting GA/CL reload\n");
		if (fpOpenArchive != nil)
			fpOpenArchive->Reload();
		if (fpContentList != nil)
			fpContentList->Reload();
	}

	/* export to registry */
	fPreferences.SaveToRegistry();

	//Invalidate();
}

/*
 * Handle IDM_EDIT_FIND.
 */
void
MainWindow::OnEditFind(void)
{
	DWORD flags = 0;

	if (fpFindDialog != nil)
		return;

	if (fFindDown)
		flags |= FR_DOWN;
	if (fFindMatchCase)
		flags |= FR_MATCHCASE;
	if (fFindMatchWholeWord)
		flags |= FR_WHOLEWORD;

	fpFindDialog = new CFindReplaceDialog;

	fpFindDialog->Create(TRUE,		// "find" only
						 fFindLastStr,	// default string to search for
						 NULL,		// default string to replace
						 flags,		// flags
						 this);		// parent
}
void
MainWindow::OnUpdateEditFind(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(fpOpenArchive != nil);
}

/*
 * Handle activity in the modeless "find" dialog.
 */
LRESULT
MainWindow::OnFindDialogMessage(WPARAM wParam, LPARAM lParam)
{
	assert(fpFindDialog != nil);

	fFindDown = (fpFindDialog->SearchDown() != 0);
	fFindMatchCase = (fpFindDialog->MatchCase() != 0);
	fFindMatchWholeWord = (fpFindDialog->MatchWholeWord() != 0);

	if (fpFindDialog->IsTerminating()) {
		fpFindDialog = nil;
		return 0;
	}

	if (fpFindDialog->FindNext()) {
		fFindLastStr = fpFindDialog->GetFindString();
		fpContentList->FindNext(fFindLastStr, fFindDown, fFindMatchCase,
			fFindMatchWholeWord);
	} else {
		WMSG0("Unexpected find dialog activity\n");
	}

	return 0;
}


/*
 * Handle IDM_SORT_*.
 *
 * The "sort" enu item should really only be active if we have a file open.
 */
void
MainWindow::OnEditSort(UINT id)
{
	WMSG1("EDIT SORT %d\n", id);

	ASSERT(id >= IDM_SORT_PATHNAME && id <= IDM_SORT_ORIGINAL);
	fPreferences.GetColumnLayout()->SetSortColumn(id - IDM_SORT_PATHNAME);
	fPreferences.GetColumnLayout()->SetAscending(true);
	if (fpContentList != nil)
		fpContentList->NewSortOrder();
}
void
MainWindow::OnUpdateEditSort(CCmdUI* pCmdUI)
{
	unsigned int column = fPreferences.GetColumnLayout()->GetSortColumn();

	pCmdUI->SetCheck(pCmdUI->m_nID - IDM_SORT_PATHNAME == column);
}

/*
 * Open the help file.
 */
void
MainWindow::OnHelpContents(void)
{
	WinHelp(0, HELP_FINDER);
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
		if (err == ERROR_FILE_NOT_FOUND) {
			msg =	"Windows call failed: web browser not found.  (Sometimes"
					" it mistakenly reports this when IE is not the default"
					" browser.)";
			ShowFailureMsg(this, msg, IDS_FAILED);
		} else {
			msg.Format("Unable to launch web browser (err=%d).", err);
			ShowFailureMsg(this, msg, IDS_FAILED);
		}
	}
}

/*
 * Show ordering info (ka-ching!).
 */
void
MainWindow::OnHelpOrdering(void)
{
	WinHelp(HELP_TOPIC_ORDERING_INFO, HELP_CONTEXT);
}

/*
 * Pop up the About box.
 */
void
MainWindow::OnHelpAbout(void)
{
	int result;

	AboutDialog dlg(this);

	result = dlg.DoModal();
	WMSG1("HelpAbout returned %d\n", result);

	/*
	 * User could've changed registration.  If we're showing the registered
	 * user name in the title bar, update it.
	 */
	if (fpOpenArchive == nil)
		SetCPTitle();
}

/*
 * Create a new SHK archive, using a "save as" dialog to select the name.
 */
void
MainWindow::OnFileNewArchive(void)
{
	CString filename, saveFolder, errStr;
	GenericArchive* pOpenArchive;
	CString errMsg;

	CFileDialog dlg(FALSE, _T("shk"), NULL,
		OFN_OVERWRITEPROMPT|OFN_NOREADONLYRETURN|OFN_HIDEREADONLY,
		"ShrinkIt Archives (*.shk)|*.shk||", this);

	dlg.m_ofn.lpstrTitle = "New Archive";
	dlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenArchiveFolder);

	if (dlg.DoModal() != IDOK)
		goto bail;

	saveFolder = dlg.m_ofn.lpstrFile;
	saveFolder = saveFolder.Left(dlg.m_ofn.nFileOffset);
	fPreferences.SetPrefString(kPrOpenArchiveFolder, saveFolder);

	filename = dlg.GetPathName();
	WMSG1("NEW FILE '%s'\n", filename);

	/* remove file if it already exists */
	errMsg = RemoveFile(filename);
	if (!errMsg.IsEmpty()) {
		ShowFailureMsg(this, errMsg, IDS_FAILED);
		goto bail;
	}

	pOpenArchive = new NufxArchive;
	errStr = pOpenArchive->New(filename, nil);
	if (!errStr.IsEmpty()) {
		CString failed;
		failed.LoadString(IDS_FAILED);
		MessageBox(errStr, failed, MB_ICONERROR);

		delete pOpenArchive;
	} else {
		SwitchContentList(pOpenArchive);
		fOpenArchivePathName = dlg.GetPathName();
		SetCPTitle(fOpenArchivePathName, fpOpenArchive);
	}

bail:
	WMSG0("--- OnFileNewArchive done\n");
}


/*
 * Handle request to open an archive or disk image.
 */
void
MainWindow::OnFileOpen(void)
{
	CString openFilters;
	CString saveFolder;

	/* set up filters; the order is significant */
	openFilters = kOpenNuFX;
	openFilters += kOpenBinaryII;
	openFilters += kOpenACU;
	openFilters += kOpenDiskImage;
	openFilters += kOpenAll;
	openFilters += kOpenEnd;
	CFileDialog dlg(TRUE, "shk", NULL,
		OFN_FILEMUSTEXIST, openFilters, this);

	dlg.m_ofn.nFilterIndex = fPreferences.GetPrefLong(kPrLastOpenFilterIndex);
	dlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenArchiveFolder);

	if (dlg.DoModal() != IDOK)
		goto bail;

	fPreferences.SetPrefLong(kPrLastOpenFilterIndex, dlg.m_ofn.nFilterIndex);
	saveFolder = dlg.m_ofn.lpstrFile;
	saveFolder = saveFolder.Left(dlg.m_ofn.nFileOffset);
	fPreferences.SetPrefString(kPrOpenArchiveFolder, saveFolder);

	DoOpenArchive(dlg.GetPathName(), dlg.GetFileExt(),
		dlg.m_ofn.nFilterIndex, dlg.GetReadOnlyPref() != 0);

bail:
	WMSG0("--- OnFileOpen done\n");
}

/*
 * Handle request to open a raw disk volume.
 */
void
MainWindow::OnFileOpenVolume(void)
{
	WMSG0("--- OnFileOpenVolume\n");

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
void
MainWindow::OnUpdateFileOpenVolume(CCmdUI* pCmdUI)
{
	// don't really need this function
	pCmdUI->Enable(TRUE);
}

/*
 * Open an archive.
 */
void
MainWindow::DoOpenArchive(const char* pathName, const char* ext,
	int filterIndex, bool readOnly)
{
	if (LoadArchive(pathName, ext, filterIndex, readOnly, false) == 0) {
		/* success, update title bar */
		fOpenArchivePathName = pathName;
		SetCPTitle(fOpenArchivePathName, fpOpenArchive);
	} else {
		/* some failures will close an open archive */
		//if (fpOpenArchive == nil)
		//	SetCPTitle();
	}
}

/*
 * Save any pending changes.
 *
 * This may be called directly from tools, so don't assume that the
 * conditions checked for in OnUpdateFileSave hold here.
 */
void
MainWindow::OnFileReopen(void)
{
	ReopenArchive();
}
void
MainWindow::OnUpdateFileReopen(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(fpOpenArchive != nil);
}


/*
 * Save any pending changes.
 *
 * This may be called directly from tools, so don't assume that the
 * conditions checked for in OnUpdateFileSave hold here.
 */
void
MainWindow::OnFileSave(void)
{
	CString errMsg;

	if (fpOpenArchive == nil)
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
void
MainWindow::OnUpdateFileSave(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(fpOpenArchive != nil && fpOpenArchive->IsModified());
}

/*
 * Close current archive or disk image.
 */
void
MainWindow::OnFileClose(void)
{
	CloseArchive();
	//SetCPTitle();
	WMSG0("--- OnFileClose done\n");
}
void
MainWindow::OnUpdateFileClose(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(fpOpenArchive != nil);
}


/*
 * Show detailed information on the current archive.
 */
void
MainWindow::OnFileArchiveInfo(void)
{
	ArchiveInfoDialog* pDlg = nil;
	ASSERT(fpOpenArchive != nil);

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
	default:
		WMSG1("Unexpected archive type %d\n", fpOpenArchive->GetArchiveKind());
		ASSERT(false);
		return;
	};

	pDlg->DoModal();

	delete pDlg;
}
void
MainWindow::OnUpdateFileArchiveInfo(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(fpContentList != nil);
}

/*
 * Print the contents of the current archive.
 */
void
MainWindow::OnFilePrint(void)
{
	PrintListing(fpContentList);
}
void
MainWindow::OnUpdateFilePrint(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(fpContentList != nil && fpContentList->GetItemCount() > 0);
}

/*
 * Print a ContentList.
 */
void
MainWindow::PrintListing(const ContentList* pContentList)
{
	CPrintDialog dlg(FALSE);	// use CPrintDialogEx for Win2K? CPageSetUpDialog?
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
		msg.LoadString(IDS_PRINTER_NOT_USABLE);
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


/*
 * Handle Exit item by sending a close request.
 */
void
MainWindow::OnFileExit(void)
{
	SendMessage(WM_CLOSE, 0, 0);
}


/*
 * Select everything in the content list.
 */
void
MainWindow::OnEditSelectAll(void)
{
	ASSERT(fpContentList != nil);
	fpContentList->SelectAll();
}
void
MainWindow::OnUpdateEditSelectAll(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(fpContentList != nil);
}

/*
 * Invert the content list selection.
 */
void
MainWindow::OnEditInvertSelection(void)
{
	ASSERT(fpContentList != nil);
	fpContentList->InvertSelection();
}
void
MainWindow::OnUpdateEditInvertSelection(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(fpContentList != nil);
}


/*
 * Get the one selected item from the current display.  Primarily useful
 * for the double-click handler, but also used for "action" menu items
 * that insist on operating on a single menu item (edit prefs, create subdir).
 *
 * Returns nil if the item couldn't be found or if more than one item was
 * selected.
 */
GenericEntry*
MainWindow::GetSelectedItem(ContentList* pContentList)
{
	if (pContentList->GetSelectedCount() != 1)
		return nil;

	POSITION posn;
	posn = pContentList->GetFirstSelectedItemPosition();
	if (posn == nil) {
		ASSERT(false);
		return nil;
	}
	int num = pContentList->GetNextSelectedItem(/*ref*/ posn);
	GenericEntry* pEntry = (GenericEntry*) pContentList->GetItemData(num);
	if (pEntry == nil) {
		WMSG1(" Glitch: couldn't find entry %d\n", num);
		ASSERT(false);
	}

	return pEntry;
}

/*
 * Handle a double-click.
 *
 * Individual items get special treatment, multiple items just get handed off
 * to the file viewer.
 */
void
MainWindow::HandleDoubleClick(void)
{
	bool handled = false;

	ASSERT(fpContentList != nil);
	if (fpContentList->GetSelectedCount() == 0) {
		/* nothing selected, they double-clicked outside first column */
		WMSG0("Double-click but nothing selected\n");
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
	if (pEntry == nil)
		return;

	WMSG1(" Double-click GOT '%s'\n", pEntry->GetPathName());
	const char* ext;
	long fileType, auxType;

	ext = FindExtension(pEntry->GetPathName(), pEntry->GetFssep());
	fileType = pEntry->GetFileType();
	auxType = pEntry->GetAuxType();

	/*	// unit tests for MatchSemicolonList
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
	if (ext != nil && MatchSemicolonList(extViewerExts, ext+1)) {
		WMSG1(" Launching external viewer for '%s'\n", ext);
		TmpExtractForExternal(pEntry);
		handled = true;
	} else if (pEntry->GetRecordKind() == GenericEntry::kRecordKindFile) {
		if ((ext != nil && (
				stricmp(ext, ".shk") == 0 ||
				stricmp(ext, ".sdk") == 0 ||
				stricmp(ext, ".bxy") == 0   )) ||
			(fileType == 0xe0 && auxType == 0x8002))
		{
			WMSG0(" Guessing NuFX\n");
			TmpExtractAndOpen(pEntry, GenericEntry::kDataThread, kModeNuFX);
			handled = true;
		} else
		if ((ext != nil && (
				stricmp(ext, ".bny") == 0 ||
				stricmp(ext, ".bqy") == 0   )) ||
			(fileType == 0xe0 && auxType == 0x8000))
		{
			WMSG0(" Guessing Binary II\n");
			TmpExtractAndOpen(pEntry, GenericEntry::kDataThread, kModeBinaryII);
			handled = true;
		} else
		if ((ext != nil && (
				stricmp(ext, ".acu") == 0   )) ||
			(fileType == 0xe0 && auxType == 0x8001))
		{
			WMSG0(" Guessing ACU\n");
			TmpExtractAndOpen(pEntry, GenericEntry::kDataThread, kModeACU);
			handled = true;
		} else
		if (fileType == 0x64496d67 && auxType == 0x64437079 &&
			pEntry->GetUncompressedLen() == 819284)
		{
			/* type is dImg, creator is dCpy, length is 800K + DC stuff */
			WMSG0(" Looks like a disk image\n");
			TmpExtractAndOpen(pEntry, GenericEntry::kDataThread, kModeDiskImage);
			handled = true;
		}
	} else if (pEntry->GetRecordKind() == GenericEntry::kRecordKindDisk) {
		WMSG0(" Opening archived disk image\n");
		TmpExtractAndOpen(pEntry, GenericEntry::kDiskImageThread, kModeDiskImage);
		handled = true;
	}

	if (!handled) {
		// standard viewer
		HandleView();
	}

	/* set "/t" temp flag and delete afterward, warning user (?) */
}

/*
 * Extract a record to the temp folder and open it with a new instance of
 * CiderPress.  We might want to extract disk images as 2MG files to take
 * the mystery out of opening them, but since they're coming out of a
 * ShrinkIt archive they're pretty un-mysterious anyway.
 *
 * We tell the new instance to open it read-only, and flag it for
 * deletion on exit.
 *
 * Returns 0 on success, nonzero error status on failure.
 */
int
MainWindow::TmpExtractAndOpen(GenericEntry* pEntry, int threadKind,
	const char* modeStr)
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

	char nameBuf[MAX_PATH];
	UINT unique;
	unique = GetTempFileName(fPreferences.GetPrefString(kPrTempPath),
				"CPfile", 0, nameBuf);
	if (unique == 0) {
		DWORD dwerr = ::GetLastError();
		WMSG2("GetTempFileName failed on '%s' (err=%ld)\n",
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

	fp = fopen(nameBuf, "wb");
	if (fp != nil) {
		WMSG2("Extracting to '%s' (unique=%d)\n", nameBuf, unique);
		result = pEntry->ExtractThreadToFile(threadKind, fp,
					GenericEntry::kConvertEOLOff, GenericEntry::kConvertHAOff,
					&errMsg);
		fclose(fp);
		if (result == IDOK) {
			/* success */
			CString parameters;

			parameters.Format("-mode %s -dispname \"%s\" -temparc \"%s\"",
				modeStr, dispName, nameBuf);
			int err;

			err = (int) ::ShellExecute(m_hWnd, _T("open"),
					gMyApp.GetExeFileName(), parameters, NULL,
							SW_SHOWNORMAL);
			if (err <= 32) {
				CString msg;
				msg.Format("Unable to launch CiderPress (err=%d).", err);
				ShowFailureMsg(this, msg, IDS_FAILED);
			} else {
				/* during dev, "missing DLL" causes false-positive success */
				WMSG0("Successfully launched CiderPress\n");
				mustDelete = false;		// up to newly-launched app
			}
		} else {
			ShowFailureMsg(this, errMsg, IDS_FAILED);
		}
	} else {
		CString msg;
		msg.Format("Unable to open temp file '%s'.", nameBuf);
		::ShowFailureMsg(this, msg, IDS_FAILED);
	}

	if (mustDelete) {
		WMSG1("Deleting '%s'\n", nameBuf);
		unlink(nameBuf);
	}

	return 0;
}

/*
 * Extract a record to the temp folder and open it with an external viewer.
 * The file must be created with the correct extension so ShellExecute
 * does the right thing.
 *
 * The files will be added to the "delete on exit" list, so that they will
 * be cleaned up when CiderPress exits (assuming the external viewer no longer
 * has them open).
 *
 * The GetTempFileName function creates a uniquely-named temp file.  We
 * create a file that has that name plus an extension.  To ensure that we
 * don't try to use the same temp filename twice, we have to hold off on
 * deleting the unused .tmp files until we're ready to delete the
 * corresponding .gif (or whatever) files.  Thus, each invocation of this
 * function creates two files and two entries in the delete-on-exit set.
 *
 * Returns 0 on success, nonzero error status on failure.
 */
int
MainWindow::TmpExtractForExternal(GenericEntry* pEntry)
{
	const char* ext;

	ext = FindExtension(pEntry->GetPathName(), pEntry->GetFssep());

	char nameBuf[MAX_PATH];
	UINT unique;
	unique = GetTempFileName(fPreferences.GetPrefString(kPrTempPath),
				"CPfile", 0, nameBuf);
	if (unique == 0) {
		DWORD dwerr = ::GetLastError();
		WMSG2("GetTempFileName failed on '%s' (err=%ld)\n",
			fPreferences.GetPrefString(kPrTempPath), dwerr);
		return dwerr;
	}
	fDeleteList.Add(nameBuf);		// file is created by GetTempFileName

	strcat(nameBuf, ext);

	/*
	 * Open the temp file and extract the data into it.
	 */
	CString errMsg;
	int result;
	FILE* fp;

	fp = fopen(nameBuf, "wb");
	if (fp != nil) {
		fDeleteList.Add(nameBuf);	// second file created by fopen
		WMSG2("Extracting to '%s' (unique=%d)\n", nameBuf, unique);
		result = pEntry->ExtractThreadToFile(GenericEntry::kDataThread, fp,
					GenericEntry::kConvertEOLOff, GenericEntry::kConvertHAOff,
					&errMsg);
		fclose(fp);
		if (result == IDOK) {
			/* success */
			int err;

			err = (int) ::ShellExecute(m_hWnd, _T("open"), nameBuf, NULL,
							NULL, SW_SHOWNORMAL);
			if (err <= 32) {
				CString msg;
				msg.Format("Unable to launch external viewer (err=%d).", err);
				ShowFailureMsg(this, msg, IDS_FAILED);
			} else {
				WMSG0("Successfully launched external viewer\n");
			}
		} else {
			ShowFailureMsg(this, errMsg, IDS_FAILED);
		}
	} else {
		CString msg;
		msg.Format("Unable to open temp file '%s'.", nameBuf);
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
void
MainWindow::OnRtClkDefault(void)
{
	int idx;

	ASSERT(fpContentList != nil);

	idx = fpContentList->GetRightClickItem();
	ASSERT(idx != -1);
	WMSG1("OnRtClkDefault %d\n", idx);

	fpContentList->ClearRightClickItem();
}
#endif


/*
 * ===================================
 *		Progress meter
 * ===================================
 */

/*
 * There are two different mechanisms for reporting progress: ActionProgress
 * dialogs (for adding/extracting files) and a small box in the lower
 * right-hand corner (for opening archives).  These functions will set
 * the progress in the active action progress dialog if it exists, or
 * will set the percentage in the window frame if not.
 */

void
MainWindow::SetProgressBegin(void)
{
	if (fpActionProgress != nil)
		fpActionProgress->SetProgress(0);
	else
		fStatusBar.SetPaneText(kProgressPane, "--%");
	//WMSG0("  Complete: BEGIN\n");

	/* redraw stuff with the changes */
	(void) PeekAndPump();
}

int
MainWindow::SetProgressUpdate(int percent, const char* oldName,
	const char* newName)
{
	int status = IDOK;

	if (fpActionProgress != nil) {
		status = fpActionProgress->SetProgress(percent);
		if (oldName != nil)
			fpActionProgress->SetArcName(oldName);
		if (newName != nil)
			fpActionProgress->SetFileName(newName);
	} else {
		char buf[8];
		sprintf(buf, "%d%%", percent);
		fStatusBar.SetPaneText(kProgressPane, buf);
		//WMSG1("  Complete: %s\n", buf);
	}

	if (!PeekAndPump()) {
		WMSG0("SetProgressUpdate: shutdown?!\n");
	}

	//EventPause(10);		// DEBUG DEBUG
	return status;
}

void
MainWindow::SetProgressEnd(void)
{
	if (fpActionProgress != nil)
		fpActionProgress->SetProgress(100);
	else
		fStatusBar.SetPaneText(kProgressPane, "");
//	EventPause(100);		// DEBUG DEBUG
	//WMSG0("  Complete: END\n");
}


/*
 * Set a number in the "progress counter".  Useful for loading large archives
 * where we're not sure how much stuff is left, so showing a percentage is
 * hard.
 *
 * Pass in -1 to erase the counter.
 *
 * Returns "true" if we'd like things to continue.
 */
bool
MainWindow::SetProgressCounter(const char* str, long val)
{
	/* if the main window is enabled, user could activate menus */
	ASSERT(!IsWindowEnabled());

	if (fpProgressCounter != nil) {
		//WMSG2("SetProgressCounter '%s' %d\n", str, val);
		CString msg;

		if (str != nil)
			fpProgressCounter->SetCounterFormat(str);
		fpProgressCounter->SetCount((int) val);
	} else {
		if (val < 0) {
			fStatusBar.SetPaneText(kProgressPane, "");
		} else {
			CString tmpStr;
			tmpStr.Format("%ld", val);
			fStatusBar.SetPaneText(kProgressPane, tmpStr);
		}
	}

	if (!PeekAndPump()) {
		WMSG0("SetProgressCounter: shutdown?!\n");
	}
	//EventPause(10);		// DEBUG DEBUG

	if (fpProgressCounter != nil)
		return !fpProgressCounter->GetCancel();
	else
		return true;
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
 * Go to sleep for a little bit, waking up 100x per second to check
 * the idle loop.
 */
void
MainWindow::EventPause(int duration)
{
	int count = duration / 10;

	for (int i = 0; i < count; i++) {
		PeekAndPump();
		::Sleep(10);
	}
}

/*
 * Printer abort procedure; allows us to abort a print job.  The DC
 * SetAbortProc() function calls here periodically.  The return value from
 * this function determine whether or not printing halts.
 *
 * This checks a global "print cancel" variable, which is set by our print
 * cancel button dialog.
 *
 * If this returns TRUE, printing continues; FALSE, and printing aborts.
 */
/*static*/ BOOL CALLBACK
MainWindow::PrintAbortProc(HDC hDC, int nCode)
{
	MainWindow* pMain = (MainWindow*)::AfxGetMainWnd();

	pMain->PeekAndPump();
	if (pMain->GetAbortPrinting()) {
		WMSG0("PrintAbortProc returning FALSE (abort printing)\n");
		return FALSE;
	}
	WMSG0("  PrintAbortProc returning TRUE (continue printing)\n");
	return TRUE;
}

/*
 * ===================================
 *		Support functions
 * ===================================
 */

/*
 * Draw what looks like an empty client area.
 */
void
MainWindow::DrawEmptyClientArea(CDC* pDC, const CRect& clientRect)
{
	CBrush brush;
	brush.CreateSolidBrush(::GetSysColor(COLOR_APPWORKSPACE));	// dk gray
	CBrush* pOldBrush = pDC->SelectObject(&brush);
	pDC->FillRect(&clientRect, &brush);
	pDC->SelectObject(pOldBrush);

	CPen penWH(PS_SOLID, 1, ::GetSysColor(COLOR_3DHIGHLIGHT));	// white
	CPen penLG(PS_SOLID, 1, ::GetSysColor(COLOR_3DLIGHT));		// lt gray
	CPen penDG(PS_SOLID, 1, ::GetSysColor(COLOR_3DSHADOW));		// dk gray
	CPen penBL(PS_SOLID, 1, ::GetSysColor(COLOR_3DDKSHADOW));	// near-black
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

/*
 * Load an archive, using the appropriate GenericArchive subclass.  If
 * "createFile" is "true", a new archive file will be created (and must
 * not already exist!).
 *
 * "filename" is the full path to the file, "extension" is the
 * filetype component of the name (without the leading '.'), "filterIndex"
 * is the offset into the set of filename filters used in the standard
 * file dialog, "readOnly" reflects the state of the stdfile dialog
 * checkbox, and "createFile" is set to true by the "New Archive" command.
 *
 * Returns 0 on success, nonzero on failure.
 */
int
MainWindow::LoadArchive(const char* fileName, const char* extension,
	int filterIndex, bool readOnly, bool createFile)
{
	GenericArchive::OpenResult openResult;
	int result = -1;
	GenericArchive* pOpenArchive = nil;
	int origFilterIndex = filterIndex;
	CString errStr, appName;

	appName.LoadString(IDS_MB_APP_NAME);

	WMSG3("LoadArchive: '%s' ro=%d idx=%d\n", fileName, readOnly, filterIndex);

	/* close any existing archive to avoid weirdness from re-open */
	CloseArchive();

	/*
	 * If they used the "All Files (*.*)" filter, we have to guess based
	 * on the file type.
	 *
	 * IDEA: change the current "filterIndex ==" stuff to a type-specific
	 * model, then do type-scanning here.  Code later on takes the type
	 * and opens it.  That way we can do the trivial "it must be" handling
	 * up here, and maybe do a little "open it up and see" stuff as well.
	 * In general, though, if we don't recognize the extension, it's
	 * probably a disk image.
	 */
	if (filterIndex == kFilterIndexGeneric) {
		int i;

		for (i = 0; i < NELEM(gExtensionToIndex); i++) {
			if (strcasecmp(extension, gExtensionToIndex[i].extension) == 0) {
				filterIndex = gExtensionToIndex[i].idx;
				break;
			}
		}

		if (i == NELEM(gExtensionToIndex))
			filterIndex = kFilterIndexDiskImage;
	}

try_again:
	if (filterIndex == kFilterIndexBinaryII) {
		/* try Binary II and nothing else */
		ASSERT(!createFile);
		WMSG0("  Trying Binary II\n");
		pOpenArchive = new BnyArchive;
		openResult = pOpenArchive->Open(fileName, readOnly, &errStr);
		if (openResult != GenericArchive::kResultSuccess) {
			if (!errStr.IsEmpty())
				ShowFailureMsg(this, errStr, IDS_FAILED);
			result = -1;
			goto bail;
		}
	} else
	if (filterIndex == kFilterIndexACU) {
		/* try ACU and nothing else */
		ASSERT(!createFile);
		WMSG0("  Trying ACU\n");
		pOpenArchive = new AcuArchive;
		openResult = pOpenArchive->Open(fileName, readOnly, &errStr);
		if (openResult != GenericArchive::kResultSuccess) {
			if (!errStr.IsEmpty())
				ShowFailureMsg(this, errStr, IDS_FAILED);
			result = -1;
			goto bail;
		}
	} else
	if (filterIndex == kFilterIndexDiskImage) {
		/* try various disk image formats */
		ASSERT(!createFile);
		WMSG0("  Trying disk images\n");

		pOpenArchive = new DiskArchive;
		openResult = pOpenArchive->Open(fileName, readOnly, &errStr);
		if (openResult == GenericArchive::kResultCancel) {
			result = -1;
			goto bail;
		} else if (openResult == GenericArchive::kResultFileArchive) {
			delete pOpenArchive;
			pOpenArchive = nil;

			if (strcasecmp(extension, "zip") == 0) {
				errStr = "ZIP archives with multiple files are not supported.";
				MessageBox(errStr, appName, MB_OK|MB_ICONINFORMATION);
				result = -1;
				goto bail;
			} else {
				/* assume some variation of a ShrinkIt archive */
				// msg.LoadString(IDS_OPEN_AS_NUFX); <-- with MB_OKCANCEL
				filterIndex = kFilterIndexNuFX;
				goto try_again;
			}

		} else if (openResult != GenericArchive::kResultSuccess) {
			if (filterIndex != origFilterIndex) {
				/*
				 * Kluge: assume we guessed disk image and were wrong.
				 */
				errStr = "File doesn't appear to be a valid archive"
						 " or disk image.";
			}
			if (!errStr.IsEmpty())
				ShowFailureMsg(this, errStr, IDS_FAILED);
			result = -1;
			goto bail;
		}
	} else
	if (filterIndex == kFilterIndexNuFX) {
		/* try NuFX (including its embedded-in-BNY form) */
		WMSG0("  Trying NuFX\n");

		pOpenArchive = new NufxArchive;
		openResult = pOpenArchive->Open(fileName, readOnly, &errStr);
		if (openResult != GenericArchive::kResultSuccess) {
			if (!errStr.IsEmpty())
				ShowFailureMsg(this, errStr, IDS_FAILED);
			result = -1;
			goto bail;
		}

	} else {
		ASSERT(FALSE);
		result = -1;
		goto bail;
	}

	SwitchContentList(pOpenArchive);

	pOpenArchive = nil;
	result = 0;

bail:
	if (pOpenArchive != nil) {
		ASSERT(result != 0);
		delete pOpenArchive;
	}
	return result;
}

/*
 * Open a raw disk volume.  Useful for ProDOS-formatted 1.44MB floppy disks
 * and CFFA flash cards.
 *
 * Assume it's a disk image -- it'd be a weird place for a ShrinkIt archive.
 * CFFA cards can actually hold multiple volumes, but that's all taken care
 * of inside the diskimg DLL.
 *
 * Returns 0 on success, nonzero on failure.
 */
int
MainWindow::DoOpenVolume(CString drive, bool readOnly)
{
	int result = -1;

	ASSERT(drive.GetLength() > 0);

	CString errStr;
	//char filename[4] = "_:\\";
	//filename[0] = driveLetter;

	WMSG2("FileOpenVolume '%s' %d\n", (const char*)drive, readOnly);

	/* close existing archive */
	CloseArchive();

	GenericArchive* pOpenArchive = nil;
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
	pOpenArchive = nil;
	fOpenArchivePathName = drive;
	result = 0;

	fOpenArchivePathName = drive;
	SetCPTitle(fOpenArchivePathName, fpOpenArchive);

bail:
	if (pOpenArchive != nil) {
		ASSERT(result != 0);
		delete pOpenArchive;
	}
	return result;
}


/*
 * Close and re-open the current archive.
 */
void
MainWindow::ReopenArchive(void)
{
	if (fpOpenArchive == nil) {
		ASSERT(false);
		return;
	}

	/* clear the flag, regardless of success or failure */
	fNeedReopen = false;

	GenericArchive* pOpenArchive = nil;
	CString pathName = fpOpenArchive->GetPathName();
	bool readOnly = fpOpenArchive->IsReadOnly();
	GenericArchive::ArchiveKind archiveKind = fpOpenArchive->GetArchiveKind();
	GenericArchive::OpenResult openResult;
	CString errStr;

	/* if the open fails we *don't* want to leave the previous content up */
	WMSG3("Reopening '%s' ro=%d kind=%d\n", pathName, readOnly, archiveKind);
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

	WMSG0(" Reopen was successful\n");
	SwitchContentList(pOpenArchive);
	pOpenArchive = nil;
	SetCPTitle(pathName, fpOpenArchive);

bail:
	delete pOpenArchive;
}

/*
 * Determine whether "path" matches the pathname of the currently open archive.
 */
bool
MainWindow::IsOpenPathName(const char* path)
{
	if (fpOpenArchive == nil)
		return false;

	if (stricmp(path, fpOpenArchive->GetPathName()) == 0)
		return true;

	return false;
}


/*
 * Switch the content list to a new archive, closing the previous if one
 * was already open.
 */
void
MainWindow::SwitchContentList(GenericArchive* pOpenArchive)
{
	assert(pOpenArchive != nil);

	/*
	 * We've got an archive opened successfully.  If we already had one
	 * open, shut it.  (This assumes that closing an archive is a simple
	 * matter of closing files and freeing storage.  If we needed to do
	 * something that might fail, like flush changes, we should've done
	 * that before getting this far to avoid confusion.)
	 */
	if (fpOpenArchive != nil)
		CloseArchive();

	ASSERT(fpOpenArchive == nil);
	ASSERT(fpContentList == nil);

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


/*
 * Close the existing archive file, but don't try to shut down the child
 * windows.  This should really only be used from the destructor.
 */
void
MainWindow::CloseArchiveWOControls(void)
{
	if (fpOpenArchive != nil) {
		//fpOpenArchive->Close();
		WMSG0("Deleting OpenArchive\n");
		delete fpOpenArchive;
		fpOpenArchive = nil;
	}
}

/*
 * Close the existing archive file, and throw out the control we're
 * using to display it.
 */
void
MainWindow::CloseArchive(void)
{
	CWaitCursor waitc;	// closing large compressed archive can be slow

	// destroy the ContentList
	if (fpContentList != nil) {
		WMSG0("Destroying ContentList\n");
		fpContentList->DestroyWindow();	// auto-cleanup invokes "delete"
		fpContentList = nil;
	}

	// destroy the GenericArchive
	CloseArchiveWOControls();

	// reset the title bar
	SetCPTitle();
}


/*
 * Set the title bar on the main window.
 *
 * "pathname" is often different from pOpenArchive->GetPathName(), especially
 * when we were launched from another instance of CiderPress and handed a
 * temp file whose name we're trying to conceal.
 */
void
MainWindow::SetCPTitle(const char* pathname, GenericArchive* pOpenArchive)
{
	ASSERT(pathname != nil);
	CString title;
	CString archiveDescription;
	CString appName;

	appName.LoadString(IDS_MB_APP_NAME);

	pOpenArchive->GetDescription(&archiveDescription);
	title.Format(_T("%s - %s (%s)"), appName, pathname, archiveDescription);

	if (fpOpenArchive->IsReadOnly()) {
		CString readOnly;
		readOnly.LoadString(IDS_READONLY);
		title += _T(" ");
		title += readOnly;
	}

	SetWindowText(title);
}

/*
 * Set the title bar to something boring when nothing is open.
 */
void
MainWindow::SetCPTitle(void)
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

	appName.LoadString(IDS_MB_APP_NAME);
	title = appName + regName;
	SetWindowText(title);
}

/*
 * Come up with a title to put at the top of a printout.  This is essentially
 * the same as the window title, but without some flags (e.g. "read-only").
 */
CString
MainWindow::GetPrintTitle(void)
{
	CString title;
	CString archiveDescription;
	CString appName;

	if (fpOpenArchive == nil) {
		ASSERT(false);
		return title;
	}

	appName.LoadString(IDS_MB_APP_NAME);

	fpOpenArchive->GetDescription(&archiveDescription);
	title.Format(_T("%s - %s (%s)"),
		appName, fOpenArchivePathName, archiveDescription);

	return title;
}


/*
 * After successful completion of a command, make a happy noise (but only
 * if we're configured to do so).
 */
void
MainWindow::SuccessBeep(void)
{
	const Preferences* pPreferences = GET_PREFERENCES();

	if (pPreferences->GetPrefBool(kPrBeepOnSuccess)) {
		WMSG0("<happy-beep>\n");
		::MessageBeep(MB_OK);
	}
}

/*
 * If something fails, make noise if we're configured for loudness.
 */
void
MainWindow::FailureBeep(void)
{
	const Preferences* pPreferences = GET_PREFERENCES();

	if (pPreferences->GetPrefBool(kPrBeepOnSuccess)) {
		WMSG0("<failure-beep>\n");
		::MessageBeep(MB_ICONEXCLAMATION);	// maybe MB_ICONHAND?
	}
}

/*
 * Remove a file.  Returns a helpful error string on failure.
 *
 * The absence of the file is not considered an error.
 */
CString
MainWindow::RemoveFile(const char* fileName)
{
	CString errMsg;

	int cc;
	cc = unlink(fileName);
	if (cc < 0 && errno != ENOENT) {
		int err = errno;
		WMSG2("Failed removing file '%s', errno=%d\n", fileName, err);
		errMsg.Format("Unable to remove '%s': %s.",
			fileName, strerror(err));
		if (err == EACCES)
			errMsg += "\n\n(Make sure the file isn't open.)";
	}

	return errMsg;
}


/*
 * Configure a ReformatHolder based on the current preferences.
 */
/*static*/ void
MainWindow::ConfigureReformatFromPreferences(ReformatHolder* pReformat)
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
}

/*
 * Convert a DiskImg format spec into a ReformatHolder SourceFormat.
 */
/*static*/ ReformatHolder::SourceFormat
MainWindow::ReformatterSourceFormat(DiskImg::FSFormat format)
{
	if (DiskImg::UsesDOSFileStructure(format))
		return ReformatHolder::kSourceFormatDOS;
	else if (format == DiskImg::kFormatCPM)
		return ReformatHolder::kSourceFormatCPM;
	else
		return ReformatHolder::kSourceFormatGeneric;
}
