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
        //fpSelSet = nil;
        //fpOptionsDlg = nil;
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

    void SetArcName(const WCHAR* str);
    void SetFileName(const WCHAR* str);
    const CString GetFileName(void);
    int SetProgress(int perc);

private:
    virtual BOOL OnInitDialog(void);

    Action          fAction;
    bool            fCancel;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_ACTIONPROGRESSDIALOG_H*/
