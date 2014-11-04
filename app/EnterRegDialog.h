/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Dialog allowing the user to enter registration data.
 */
#ifndef __ENTERREGDIALOG__
#define __ENTERREGDIALOG__

#include "../util/UtilLib.h"
#include "resource.h"

/*
 * Straightforward dialog.  We validate the registration key in the DDX
 * function, so an IDOK is a guarantee that they have entered valid data.  It
 * is up to the caller to store the values in the registry.
 */
class EnterRegDialog : public CDialog {
public:
    EnterRegDialog(CWnd* pParent = nil) : CDialog(IDD_REGISTRATION, pParent)
    { fDepth = 0; }
    virtual ~EnterRegDialog(void) {}

    CString     fUserName;
    CString     fCompanyName;
    CString     fRegKey;

    static int GetRegInfo(CWnd* pWnd);

private:
    // overrides
    virtual BOOL OnInitDialog(void);
    virtual void DoDataExchange(CDataExchange* pDX);

    afx_msg void OnUserChange(void);
    afx_msg void OnCompanyChange(void);
    afx_msg void OnRegChange(void);
    afx_msg void OnHelp(void);

    void HandleEditChange(int editID, int crcID);

    MyEdit      fMyEdit;
    int         fDepth;

    DECLARE_MESSAGE_MAP()
};

#endif /*__ENTERREGDIALOG__*/