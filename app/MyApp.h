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

/* CiderPress version numbers */
#define kAppMajorVersion    4
#define kAppMinorVersion    0
#define kAppBugVersion      3
#define kAppDevString       L""

/*
 * Windows application object.
 */
class MyApp: public CWinAppEx
{
public:
    MyApp();
    virtual ~MyApp(void);

#ifdef CAN_UPDATE_FILE_ASSOC
    MyRegistry  fRegistry;
#endif

    const WCHAR* GetExeFileName(void) const { return fExeFileName; }
    const WCHAR* GetExeBaseName(void) const { return fExeBaseName; }

    /*
     * Handles pop-up help requests.  Call this from OnHelpInfo.
     */
    static BOOL HandleHelpInfo(HELPINFO* lpHelpInfo);

    /*
     * Handles help topic requests.  Call this from OnHelp.
     */
    static void HandleHelp(CWnd* pWnd, DWORD topicId);

private:
    virtual BOOL InitInstance(void) override;
    virtual BOOL OnIdle(LONG lCount) override;

    /*
     * Show where we got something from.  Handy for checking DLL load locations.
     *
     * If "name" is NULL, we show the EXE info.
     */
    void LogModuleLocation(const WCHAR* name);

    /*
     * This holds pairs of control IDs and popup help IDs, for use by
     * HtmlHelp HH_TP_HELP_WM_HELP.
     *
     * The control and help ID values are identical just to make life
     * simpler, but we need a table anyway.
     */
    static const DWORD PopUpHelpIds[];

    CString     fExeFileName;
    CString     fExeBaseName;
};

extern MyApp gMyApp;

#endif /*APP_MYAPP_H*/
