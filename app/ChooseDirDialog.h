/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Dialog for choosing a directory.
 */
#ifndef APP_CHOOSEDIRDIALOG
#define APP_CHOOSEDIRDIALOG

#include <afxshellmanager.h>


/*
 * Choose a directory.  This is distinctly different from what the standard
 * "Open" and "Save As" dialogs do, because those want to choose normal files
 * only, while this wants to select a folder.
 *
 * Win2K added the shell "browse for folder" dialog, which does exactly
 * what we want.
 */
class ChooseDirDialog {
public:
    ChooseDirDialog(CWnd* pParent = NULL) {
        fpParent = pParent;
    }
    ~ChooseDirDialog() {}

    // Gets the pathname.  Call this after DoModal has updated it.
    const CString& GetPathName(void) const {
        return fPathName;
    }

    // Sets the pathname.  Call before DoModal().
    void SetPathName(const CString& str) {
        fPathName = str;
    }

    // Returns false if nothing was selected (e.g. the dialog was canceled).
    BOOL DoModal() {
        CShellManager* pMan = gMyApp.GetShellManager();
        CString outFolder;
        BOOL result = pMan->BrowseForFolder(outFolder, fpParent, fPathName,
            L"Select folder:", BIF_RETURNONLYFSDIRS | BIF_USENEWUI);
        fPathName = outFolder;
        return result;
    }

private:
    CWnd* fpParent;
    CString fPathName;
};

#endif /*APP_CHOOSEDIRDIALOG*/
