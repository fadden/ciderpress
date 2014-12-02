/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * The application object.
 */
#include "stdafx.h"
#include "mdc.h"
#include "Main.h"
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
    // TODO: make the log file configurable
    gDebugLog = new DebugLog(L"C:\\src\\mdclog.txt");

    time_t now = time(NULL);
    LOGI("MDC v%d.%d.%d started at %.24hs",
        kAppMajorVersion, kAppMinorVersion, kAppBugVersion,
        ctime(&now));

    int tmpDbgFlag;
    // enable memory leak detection
    tmpDbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    tmpDbgFlag |= _CRTDBG_LEAK_CHECK_DF;
    _CrtSetDbgFlag(tmpDbgFlag);
    LOGI("Leak detection enabled");
}

/*
 * This is the last point of control we have.
 */
MyApp::~MyApp(void)
{
    LOGI("MDC shutting down");
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
    m_pMainWnd = new MainWindow;
    m_pMainWnd->ShowWindow(m_nCmdShow);
    m_pMainWnd->UpdateWindow();

    return TRUE;
}
