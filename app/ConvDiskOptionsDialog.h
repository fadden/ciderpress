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
    virtual BOOL OnInitDialog(void) override;
    virtual void DoDataExchange(CDataExchange* pDX) override;

    /*
     * Enable all size radio buttons and reset the "size required" display.
     *
     * This should be invoked whenever the convert selection changes, and may be
     * called at any time.
     */
    afx_msg void ResetSizeControls(void);

    /*
     * Compute the amount of space required for the files.  We use the result to
     * disable the controls that can't be used.
     *
     * We don't need to enable controls here, because the only way to change the
     * set of files is by flipping between "all" and "selected", and we can handle
     * that separately.
     */
    afx_msg void OnCompute(void);

    /*
     * When one of the radio buttons is clicked on, update the active status
     * and contents of the "specify size" edit box.
     */
    afx_msg void OnRadioChangeRange(UINT nID);

    /*
     * Display the space requirements and disable radio button controls that are
     * for values that are too small.
     *
     * Pass in the number of blocks required on a 32MB ProDOS volume.
     */
    void LimitSizeControls(long totalBlocks, long blocksUsed);

    /*
     * Test a ProDOS filename for validity.
     */
    bool IsValidVolumeName_ProDOS(const WCHAR* name);

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_CONVDISKOPTIONSDIALOG_H*/
