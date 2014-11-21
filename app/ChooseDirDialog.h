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

#include "../util/UtilLib.h"
#include "resource.h"

/*
 * Choose a directory.  This is distinctly different from what the standard
 * "Open" and "Save As" dialogs do, because those want to choose normal files
 * only, while this wants to select a folder.
 */
class ChooseDirDialog : public CDialog {
public:
    ChooseDirDialog(CWnd* pParent = NULL, int dialogID = IDD_CHOOSEDIR) :
        CDialog(dialogID, pParent)
    {
        fPathName = L"";
    }
    virtual ~ChooseDirDialog(void) {}

    const WCHAR* GetPathName(void) const { return fPathName; }

    // set the pathname; when DoModal is called this will tunnel in
    void SetPathName(const WCHAR* str) { fPathName = str; }

protected:
    virtual BOOL OnInitDialog(void) override;

    // Special handling for "return" key.
    virtual BOOL PreTranslateMessage(MSG* pMsg) override;

    /*
     * Replace the ShellTree's default SELCHANGED handler with this so we can
     * track changes to the edit control.
     */
    afx_msg void OnSelChanged(NMHDR* pnmh, LRESULT* pResult);

    // F1 key hit, or '?' button in title bar used to select help for an
    // item in the dialog.  For ON_WM_HELPINFO.
    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo);

    // User pressed "Expand Tree" button.
    afx_msg void OnExpandTree(void);

    // User pressed "New Folder" button.
    afx_msg void OnNewFolder(void);

    // User pressed "Help" button.
    afx_msg void OnHelp(void);

private:
    CString         fPathName;

    ShellTree       fShellTree;
    MyBitmapButton  fNewFolderButton;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_CHOOSEDIRDIALOG*/
