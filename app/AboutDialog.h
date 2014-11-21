/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Class definition for About dialog.
 */
#ifndef APP_ABOUTDIALOG_H
#define APP_ABOUTDIALOG_H

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
    /*
     * Update the static strings with DLL version numbers.
     */
    virtual BOOL OnInitDialog(void) override;

    /*
     * User hit the "Credits" button.
     */
    afx_msg void OnAboutCredits(void);

    //afx_msg void OnEnterReg(void);
    //void ShowRegistrationInfo(void);

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_ABOUTDIALOG_H*/
