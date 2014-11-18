/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * The application object.
 */
#include "stdafx.h"
#include "../util/UtilLib.h"
#include "MyApp.h"
#include "Registry.h"
#include "Main.h"
#include "DiskArchive.h"
#include <process.h>

/* magic global that MFC finds (or that finds MFC) */
MyApp gMyApp;

DebugLog* gDebugLog;


/*
 * Constructor.  This is the closest thing to "main" that we have, but we
 * should wait for InitInstance for most things.
 */
MyApp::MyApp(LPCTSTR lpszAppName) : CWinApp(lpszAppName)
{
    gDebugLog = new DebugLog(L"C:\\src\\cplog.txt");

    time_t now = time(NULL);

    LOGI("CiderPress v%d.%d.%d%ls started at %.24hs\n",
        kAppMajorVersion, kAppMinorVersion, kAppBugVersion,
        kAppDevString, ctime(&now));

    int tmpDbgFlag;
    // enable memory leak detection
    tmpDbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    tmpDbgFlag |= _CRTDBG_LEAK_CHECK_DF;
    _CrtSetDbgFlag(tmpDbgFlag);
    WMSG0("Leak detection enabled\n");
}

/*
 * This is the last point of control we have.
 */
MyApp::~MyApp(void)
{
    DiskArchive::AppCleanup();
    NiftyList::AppCleanup();

    WMSG0("SHUTTING DOWN\n\n");
    delete gDebugLog;
}


/*
 * It all begins here.
 *
 * Create a main window.
 */
BOOL
MyApp::InitInstance(void)
{
    //fclose(fopen("c:\\cp-initinstance.txt", "w"));

    //_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);

    m_pMainWnd = new MainWindow;
    m_pMainWnd->ShowWindow(m_nCmdShow);
    m_pMainWnd->UpdateWindow();

    WMSG0("Happily in InitInstance!\n");

    /* find our .EXE file */
    //HMODULE hModule = ::GetModuleHandle(NULL);
    WCHAR buf[MAX_PATH];
    if (::GetModuleFileName(NULL /*hModule*/, buf, NELEM(buf)) != 0) {
        WMSG1("Module name is '%ls'\n", buf);
        fExeFileName = buf;

        WCHAR* cp = wcsrchr(buf, '\\');
        if (cp == NULL)
            fExeBaseName = L"";
        else
            fExeBaseName = fExeFileName.Left(cp - buf +1);
    } else {
        WMSG1("BIG problem: GetModuleFileName failed (err=%ld)\n",
            ::GetLastError());
    }

    LogModuleLocation(L"riched.dll");
    LogModuleLocation(L"riched20.dll");
    LogModuleLocation(L"riched32.dll");

#if 0
    /* find our .INI file by tweaking the EXE path */
    char* cp = strrchr(buf, '\\');
    if (cp == NULL)
        cp = buf;
    else
        cp++;
    if (cp + ::lstrlen(_T("CiderPress.INI")) >= buf+sizeof(buf))
        return FALSE;
    ::lstrcpy(cp, _T("CiderPress.INI"));

    free((void*)m_pszProfileName);
    m_pszProfileName = strdup(buf);
    WMSG1("Profile name is '%ls'\n", m_pszProfileName);

    if (!WriteProfileString("SectionOne", "MyEntry", "test"))
        WMSG0("WriteProfileString failed\n");
#endif

    SetRegistryKey(fRegistry.GetAppRegistryKey());

    //WMSG1("Registry key is '%ls'\n", m_pszRegistryKey);
    //WMSG1("Profile name is '%ls'\n", m_pszProfileName);
    WMSG1("Short command line is '%ls'\n", m_lpCmdLine);
    //WMSG1("CP app name is '%ls'\n", m_pszAppName);
    //WMSG1("CP exe name is '%ls'\n", m_pszExeName);
    WMSG1("CP help file is '%ls'\n", m_pszHelpFilePath);
    WMSG1("Command line is '%ls'\n", ::GetCommandLine());

    //if (!WriteProfileString("SectionOne", "MyEntry", "test"))
    //  WMSG0("WriteProfileString failed\n");

    /*
     * If we're installing or uninstalling, do what we need to and then
     * bail immediately.  This will hemorrhage memory, but I'm sure the
     * incredibly robust Windows environment will take it in stride.
     */
    if (wcscmp(m_lpCmdLine, L"-install") == 0) {
        WMSG0("Invoked with INSTALL flag\n");
        fRegistry.OneTimeInstall();
        exit(0);
    } else if (wcscmp(m_lpCmdLine, L"-uninstall") == 0) {
        WMSG0("Invoked with UNINSTALL flag\n");
        fRegistry.OneTimeUninstall();
        exit(1);    // tell DeployMaster to continue with uninstall
    }

    fRegistry.FixBasicSettings();

    return TRUE;
}

/*
 * Show where we got something from.  Handy for checking DLL load locations.
 *
 * If "name" is NULL, we show the EXE info.
 */
void
MyApp::LogModuleLocation(const WCHAR* name)
{
    HMODULE hModule;
    WCHAR fileNameBuf[256];
    hModule = ::GetModuleHandle(name);
    if (hModule != NULL &&
        ::GetModuleFileName(hModule, fileNameBuf, NELEM(fileNameBuf)) != 0)
    {
        // GetModuleHandle does not increase ref count, so no need to release
        WMSG2("Module '%ls' loaded from '%ls'\n", name, fileNameBuf);
    } else {
        WMSG1("Module '%ls' not loaded\n", name);
    }
}

/*
 * Do some idle processing.
 */
BOOL
MyApp::OnIdle(LONG lCount)
{
    BOOL bMore = CWinApp::OnIdle(lCount);

    //if (lCount == 0) {
    //  WMSG1("IDLE lcount=%d\n", lCount);
    //}

    /*
     * If MFC is done, we take a swing.
     */
    if (bMore == false) {
        /* downcast */
        ((MainWindow*)m_pMainWnd)->DoIdle();
    }

    return bMore;
}
