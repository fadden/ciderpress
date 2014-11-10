/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * The application object.
 */
#ifndef APP_MYAPP_H
#define APP_MYAPP_H

#include "Registry.h"

#if defined(_DEBUG_LOG)
//#define kDebugLog "C:\\test\\cplog.txt"
#define kDebugLog   L"C:\\cplog.txt"
#endif

/* CiderPress version numbers */
#define kAppMajorVersion    3
#define kAppMinorVersion    0
#define kAppBugVersion      1
#define kAppDevString       L""

/*
 * Windows application object.
 */
class MyApp: public CWinApp
{
public:
    MyApp(LPCTSTR lpszAppName = NULL);
    virtual ~MyApp(void);

    MyRegistry  fRegistry;

    const WCHAR* GetExeFileName(void) const { return fExeFileName; }
    const WCHAR* GetExeBaseName(void) const { return fExeBaseName; }

private:
    // Overridden functions
    virtual BOOL InitInstance(void);
    virtual BOOL OnIdle(LONG lCount);

    void LogModuleLocation(const WCHAR* name);

    CString     fExeFileName;
    CString     fExeBaseName;
};

extern MyApp gMyApp;

#endif /*APP_MYAPP_H*/
