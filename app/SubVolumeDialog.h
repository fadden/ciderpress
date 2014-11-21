/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Sub-volume selection dialog.
 */
#ifndef APP_SUBVOLUMEDIALOG_H
#define APP_SUBVOLUMEDIALOG_H

#include "resource.h"
#include "../diskimg/DiskImg.h"
using namespace DiskImgLib;

/*
 * Display the sub-volume selection dialog, which is primarily a list box
 * with the sub-volumes listed in it.
 */
class SubVolumeDialog : public CDialog {
public:
    SubVolumeDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_SUBV, pParentWnd)
    {
        fListBoxIndex = 0;
    }
    virtual ~SubVolumeDialog(void) {}

    void Setup(DiskFS* pDiskFS) { fpDiskFS = pDiskFS; }

    /* so long as we don't sort the list, this number is enough */
    int         fListBoxIndex;

protected:
    virtual BOOL OnInitDialog(void) override;
    virtual void DoDataExchange(CDataExchange* pDX) override;

    afx_msg void OnItemDoubleClicked(void);

private:
    DiskFS*     fpDiskFS;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_SUBVOLUMEDIALOG_H*/
