/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#if !defined(AFX_MDC_H__15BEB7EF_BB49_4E23_BDD1_7F7B0220F3DB__INCLUDED_)
#define AFX_MDC_H__15BEB7EF_BB49_4E23_BDD1_7F7B0220F3DB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "resource.h"

/* MDC version numbers */
#define kAppMajorVersion    3
#define kAppMinorVersion    0
#define kAppBugVersion      0

/*
 * Windows application object.
 */
class MyApp : public CWinApp {
public:
    MyApp(LPCTSTR lpszAppName = NULL);
    virtual ~MyApp(void);

    // Overridden functions
    virtual BOOL InitInstance(void);
    //virtual BOOL OnIdle(LONG lCount);
};

extern MyApp gMyApp;

#endif // !defined(AFX_MDC_H__15BEB7EF_BB49_4E23_BDD1_7F7B0220F3DB__INCLUDED_)
