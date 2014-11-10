/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Options for converting a disk image to a file archive.
 */
#ifndef APP_CONVDISKOPTIONSDIALOG_H
#define APP_CONVDISKOPTIONSDIALOG_H

#include "UseSelectionDialog.h"
#include "resource.h"

/*
 * Get some options.
 */
class ConvDiskOptionsDialog : public UseSelectionDialog {
public:
    ConvDiskOptionsDialog(int selCount, CWnd* pParentWnd = NULL) :
        UseSelectionDialog(selCount, pParentWnd, IDD_CONVDISK_OPTS)
    {
        fDiskSizeIdx = 0;
        //fAllowLower = fSparseAlloc = FALSE;
        fVolName = L"NEW.DISK";
        fNumBlocks = -1;
    }
    virtual ~ConvDiskOptionsDialog(void) {}

    int     fDiskSizeIdx;
    //BOOL  fAllowLower;
    //BOOL  fSparseAlloc;
    CString fVolName;

    long    fNumBlocks;     // computed when DoModal finishes

private:
    virtual BOOL OnInitDialog(void);
    virtual void DoDataExchange(CDataExchange* pDX);

//  BOOL OnHelpInfo(HELPINFO* lpHelpInfo);
    afx_msg void ResetSizeControls(void);
    afx_msg void OnCompute(void);

    afx_msg void OnRadioChangeRange(UINT nID);

    void LimitSizeControls(long totalBlocks, long blocksUsed);
    bool IsValidVolumeName_ProDOS(const WCHAR* name);

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_CONVDISKOPTIONSDIALOG_H*/
