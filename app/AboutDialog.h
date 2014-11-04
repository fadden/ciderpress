/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Class definition for About dialog.
 */
#ifndef __ABOUT_DIALOG__
#define __ABOUT_DIALOG__

//#include <afxwin.h>
#include "resource.h"

/*
 * A simple dialog with an overridden initialization so we can tweak the
 * controls slightly.
 */
class AboutDialog : public CDialog {
public:
    AboutDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_ABOUTDLG, pParentWnd)
        {}

protected:
    // overrides
    virtual BOOL OnInitDialog(void);

    afx_msg void OnAboutCredits(void);
    afx_msg void OnEnterReg(void);

    //void ShowRegistrationInfo(void);

    DECLARE_MESSAGE_MAP()
};

#endif /*__ABOUT_DIALOG__*/