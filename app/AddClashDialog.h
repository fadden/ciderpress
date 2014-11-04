/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Resolve a filename clash when adding files.
 */
#ifndef __ADDCLASHDIALOG__
#define __ADDCLASHDIALOG__

/*
 *
 */
class AddClashDialog : public CDialog {
public:
    AddClashDialog(CWnd* pParentWnd = nil) :
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

    virtual BOOL OnInitDialog(void);

    DECLARE_MESSAGE_MAP()
};

#endif /*__ADDCLASHDIALOG__*/
