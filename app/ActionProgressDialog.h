/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Show the progress of an action like "add" or "extract".
 */
#ifndef APP_ACTIONPROGRESSDIALOG_H
#define APP_ACTIONPROGRESSDIALOG_H

#include "resource.h"

/*
 * Modeless dialog; must be allocated on the heap.
 */
class ActionProgressDialog : public ProgressCancelDialog {
public:
    typedef enum {
        kActionUnknown = 0,
        kActionAdd,
        kActionAddDisk,
        kActionExtract,
        kActionDelete,
        kActionTest,
        kActionRecompress,
        kActionConvDisk,
        kActionConvFile,
    } Action;

    ActionProgressDialog(void) {
        fAction = kActionUnknown;
        //fpSelSet = NULL;
        //fpOptionsDlg = NULL;
        fCancel = false;
        //fResult = 0;
    }
    virtual ~ActionProgressDialog(void) {}

    BOOL Create(Action action, CWnd* pParentWnd = NULL) {
        fAction = action;
        pParentWnd->EnableWindow(FALSE);
        return ProgressCancelDialog::Create(&fCancel, IDD_ACTION_PROGRESS,
            IDC_PROG_PROGRESS, pParentWnd);
    }
    void Cleanup(CWnd* pParentWnd) {
        pParentWnd->EnableWindow(TRUE);
        DestroyWindow();
    }

    /*
     * Set the name of the file as it appears in the archive.
     */
    void SetArcName(const WCHAR* str);

    /*
     * Set the name of the file as it appears under Windows.
     */
    void SetFileName(const WCHAR* str);

    /*
     * Get the name of the file as it appears under Windows.
     */
    const CString GetFileName(void);

    /*
     * Update the progress meter.
     *
     * We take a percentage, but the underlying control uses 1000ths.
     */
    int SetProgress(int perc);

private:
    /*
     * Initialize the static text controls to say something reasonable.
     */
    virtual BOOL OnInitDialog(void) override;

    Action          fAction;
    bool            fCancel;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_ACTIONPROGRESSDIALOG_H*/
