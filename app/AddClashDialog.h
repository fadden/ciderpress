/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#ifndef APP_ADDCLASHDIALOG_H
#define APP_ADDCLASHDIALOG_H

/*
 * Dialog for resolving a filename clash.
 */
class AddClashDialog : public CDialog {
public:
    AddClashDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_ADD_CLASH, pParentWnd)
    {
        fDoRename = false;
    }
    ~AddClashDialog(void) {}

    CString fWindowsName;
    CString fStorageName;

    bool    fDoRename;      // if "false", skip this file
    CString fNewName;

private:
    afx_msg void OnRename(void);
    afx_msg void OnSkip(void);

    virtual BOOL OnInitDialog(void) override;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_ADDCLASHDIALOG_H*/
