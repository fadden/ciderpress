/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Choose the sub-volume and directory where added files will be put.
 */
#ifndef __CHOOSE_ADD_TARGET_DIALOG__
#define __CHOOSE_ADD_TARGET_DIALOG__

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
        fpDiskFS = fpChosenDiskFS = nil;
        fpChosenSubdir = nil;
    }
    virtual ~ChooseAddTargetDialog(void) {}

    /* set this before calling DoModal */
    DiskImgLib::DiskFS* fpDiskFS;

    /* results; fpChosenSubdir will be nil if root vol selected */
    DiskImgLib::DiskFS* fpChosenDiskFS;
    DiskImgLib::A2File* fpChosenSubdir;

private:
    virtual BOOL OnInitDialog(void);
    virtual void DoDataExchange(CDataExchange* pDX);
    afx_msg void OnHelp(void);

    DiskFSTree      fDiskFSTree;

    DECLARE_MESSAGE_MAP()
};

#endif /*__CHOOSE_ADD_TARGET_DIALOG__*/
