/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Ask for confirmation before overwriting a file.
 */
#ifndef __CONFIRMOVERWRITEDIALOG__
#define __CONFIRMOVERWRITEDIALOG__

#include "resource.h"

/*
 * Accept or reject overwriting an existing file or archive record.
 */
class ConfirmOverwriteDialog : public CDialog {
public:
    ConfirmOverwriteDialog(CWnd* pParentWnd = nil) :
        CDialog(IDD_CONFIRM_OVERWRITE, pParentWnd)
    {
        fResultOverwrite = false;
        fResultApplyToAll = false;
        fResultRename = false;
        fAllowRename = true;
        fNewFileModWhen = -1;
        fExistingFileModWhen = -1;
    }
    ~ConfirmOverwriteDialog(void) {}

    // name of file in archive (during extraction) or disk (for add)
    CString     fNewFileSource;
    time_t      fNewFileModWhen;

    // full path of file being extracted onto (or record name for add)
    CString     fExistingFile;
    time_t      fExistingFileModWhen;

    // result flags: yes/no/yes-all/no-all
    bool        fResultOverwrite;
    bool        fResultApplyToAll;
    // if this flag is set, try again with updated "fExistingFile" value
    bool        fResultRename;
    // set this to enable the "Rename" button
    bool        fAllowRename;

private:
    afx_msg void OnYes(void);
    afx_msg void OnYesToAll(void);
    afx_msg void OnNo(void);
    afx_msg void OnNoToAll(void);
    afx_msg void OnRename(void);
    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo);

    virtual BOOL OnInitDialog(void);

    DECLARE_MESSAGE_MAP()
};

/*
 * Allow the user to rename a file being added or extracted, rather than
 * overwriting an existing file.  ConfirmOverwriteDialog creates one of these
 * when the "rename" button is clicked on.
 *
 * The names of the fields here correspond directly to those in
 * ConfirmOverwriteDialog.
 */
class RenameOverwriteDialog : public CDialog {
public:
    RenameOverwriteDialog(CWnd* pParentWnd = nil) :
        CDialog(IDD_RENAME_OVERWRITE, pParentWnd)
    {}
    ~RenameOverwriteDialog(void) {}

    // name of file on source medium
    CString     fNewFileSource;

    // converted name, which already exists in destination medium
    CString     fExistingFile;

    // result: what the user has renamed it to
    CString     fNewName;

private:
    virtual BOOL OnInitDialog(void);
    virtual void DoDataExchange(CDataExchange* pDX);

    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo);

    DECLARE_MESSAGE_MAP()
};

#endif /*__CONFIRMOVERWRITEDIALOG__*/