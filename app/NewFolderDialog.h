/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Allow the user to create a new folder.
 */
#ifndef APP_NEWFOLDERDIALOG_H
#define APP_NEWFOLDERDIALOG_H

#include "resource.h"

/*
 * Create a new folder in an existing location.
 *
 * Expects, but does not verify, that "fCurrentFolder" is set to a valid
 * path before DoModal is called.
 */
class NewFolderDialog : public CDialog {
public:
    NewFolderDialog(CWnd* pParent = NULL) : CDialog(IDD_NEWFOLDER, pParent) {
        fCurrentFolder = L"";
        fNewFolder = L"";
        fFolderCreated = false;
    }
    virtual ~NewFolderDialog(void) {}

    bool GetFolderCreated(void) const { return fFolderCreated; }

    // set to CWD before calling DoModal
    CString     fCurrentFolder;

    // filename (NOT pathname) of new folder (DDXed in edit ctrl)
    CString     fNewFolder;

    // full pathname of new folder, valid if fFolderCreated is true
    CString     fNewFullPath;

protected:
    void DoDataExchange(CDataExchange* pDX) override;

    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo) {
        return MyApp::HandleHelpInfo(lpHelpInfo);
    }

    // on exit, set to "true" if we created the folder in "fNewFolder"
    bool        fFolderCreated;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_NEWFOLDERDIALOG_H*/
