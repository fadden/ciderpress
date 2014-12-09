/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Choose the sub-volume and directory where added files will be put.
 */
#ifndef APP_CHOOSEADDTARGETDIALOG_H
#define APP_CHOOSEADDTARGETDIALOG_H

#include "resource.h"
#include "DiskFSTree.h"
#include "../diskimg/DiskImg.h"

/*
 * The dialog has a tree structure representing the sub-volumes and the
 * directory structure within each sub-volume.
 */
class ChooseAddTargetDialog : public CDialog {
public:
    ChooseAddTargetDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_CHOOSE_ADD_TARGET, pParentWnd)
    {
        fpDiskFS = fpChosenDiskFS = NULL;
        fpChosenSubdir = NULL;
    }
    virtual ~ChooseAddTargetDialog(void) {}

    /* set this before calling DoModal */
    DiskImgLib::DiskFS* fpDiskFS;

    /* results; fpChosenSubdir will be NULL if root vol selected */
    DiskImgLib::DiskFS* fpChosenDiskFS;
    DiskImgLib::A2File* fpChosenSubdir;

private:
    /*
     * Initialize the dialog box.  This requires scanning the provided disk
     * archive.
     */
    virtual BOOL OnInitDialog(void) override;

    virtual void DoDataExchange(CDataExchange* pDX) override;

    afx_msg void OnHelp(void) {
        MyApp::HandleHelp(this, HELP_TOPIC_CHOOSE_TARGET);
    }

    DiskFSTree      fDiskFSTree;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_CHOOSEADDTARGETDIALOG_H*/
