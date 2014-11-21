/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Show the progress of something that has no definite bound.  Because we
 * don't know when we need to stop, we just count upward.
 */
#ifndef APP_PROGRESSCOUNTERDIALOG_H
#define APP_PROGRESSCOUNTERDIALOG_H

#include "resource.h"

/*
 * Modeless dialog; must be allocated on the heap.
 */
class ProgressCounterDialog : public CancelDialog {
public:
    BOOL Create(const CString& descr, CWnd* pParentWnd = NULL) {
        fpParentWnd = pParentWnd;
        fDescr = descr;
        fCountFormat = L"%d";
        fCancel = false;

        /* disable the parent window before we're created */
        if (pParentWnd != NULL)
            pParentWnd->EnableWindow(FALSE);
        return CancelDialog::Create(&fCancel, IDD_PROGRESS_COUNTER,
                    pParentWnd);
    }

    /* enable the parent window before we're destroyed */
    virtual BOOL DestroyWindow(void) {
        if (fpParentWnd != NULL)
            fpParentWnd->EnableWindow(TRUE);
        return ModelessDialog::DestroyWindow();
    }

    /* set a format string, e.g. "Processing file %d" */
    void SetCounterFormat(const CString& fmt) { fCountFormat = fmt; }

    /* set the current count */
    void SetCount(int count) {
        CString msg;
        msg.Format(fCountFormat, count);
        GetDlgItem(IDC_PROGRESS_COUNTER_COUNT)->SetWindowText(msg);
    }

    /* get the status of the "cancelled" flag */
    bool GetCancel(void) const { return fCancel; }

private:
    BOOL OnInitDialog(void) override {
        CancelDialog::OnInitDialog();

        CWnd* pWnd = GetDlgItem(IDC_PROGRESS_COUNTER_DESC);
        pWnd->SetWindowText(fDescr);
        pWnd = GetDlgItem(IDC_PROGRESS_COUNTER_COUNT);
        pWnd->SetWindowText(L"");
        pWnd->SetFocus();           // get focus off of the Cancel button
        return FALSE;               // accept our focus
    }

    CWnd*           fpParentWnd;
    CString         fDescr;
    CString         fCountFormat;
    bool            fCancel;
};

#endif /*APP_PROGRESSCOUNTERDIALOG_H*/
