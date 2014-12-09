/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Declarations for "rename volume" dialog.
 *
 * Show a tree with possible volumes and sub-volumes, and ask the user to
 * enter the desired name (or volume number).
 *
 * We need to have the tree, rather than just clicking on an entry in the file
 * list, because we want to be able to change names and volume numbers on
 * disks with no files.
 */
#ifndef APP_RENAMEVOLUME_H
#define APP_RENAMEVOLUME_H

#include "DiskFSTree.h"
#include "resource.h"

class DiskArchive;

/*
 * Get a pointer to the DiskFS that we're altering, and a valid string for
 * the new volume name.
 */
class RenameVolumeDialog : public CDialog {
public:
    RenameVolumeDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_RENAME_VOLUME, pParentWnd)
    {
        fpArchive = NULL;
    }
    virtual ~RenameVolumeDialog(void) {}

    const DiskArchive*  fpArchive;
    CString             fNewName;
    DiskImgLib::DiskFS* fpChosenDiskFS;

protected:
    virtual BOOL OnInitDialog(void) override;
    virtual void DoDataExchange(CDataExchange* pDX) override;

    /*
     * Get a notification whenever the selection changes.  Use it to stuff a
     * default value into the edit box.
     */
    afx_msg void OnSelChanged(NMHDR* pnmh, LRESULT* pResult);

    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo) {
        return MyApp::HandleHelpInfo(lpHelpInfo);
    }
    afx_msg void OnHelp(void) {
        MyApp::HandleHelp(this, HELP_TOPIC_RENAME_VOLUME);
    }

    DiskFSTree  fDiskFSTree;

private:

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_RENAMEVOLUME_H*/
