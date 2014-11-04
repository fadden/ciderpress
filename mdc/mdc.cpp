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

#if defined(_DEBUG_LOG)
FILE* gLog = nil;
int gPid = -1;
#endif

/*
 * Constructor.  This is the closest thing to "main" that we have, but we
 * should wait for InitInstance for most things.
 */
MyApp::MyApp(LPCTSTR lpszAppName) : CWinApp(lpszAppName)
{
    time_t now;
    now = time(nil);

#ifdef _DEBUG_LOG
    gLog = fopen(kDebugLog, "w");
    if (gLog == nil)
        abort();
    ::setvbuf(gLog, nil, _IONBF, 0);

    gPid = ::getpid();
    fprintf(gLog, "\n");
#endif

    WMSG1("MDC started at %.24s\n", ctime(&now));

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
    WMSG0("MDC SHUTTING DOWN\n\n");
#ifdef _DEBUG_LOG
    if (gLog != nil)
        fclose(gLog);
#endif
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
