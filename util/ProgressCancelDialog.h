/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Display a modeless progress dialog with a "cancel" button.
 *
 * The user of this class must define a dialog with at least an IDCANCEL
 * button.
 */
#ifndef UTIL_PROCESSCANCELDIALOG_H
#define UTIL_PROCESSCANCELDIALOG_H

/*
 * This class must be allocated on the heap.
 *
 * Create with the dialog ID, thermometer ID, parent window, and a pointer to
 * a boolean that will be set to "true" if the user hits "cancel".
 */
class ProgressCancelDialog : public CancelDialog {
public:
    ProgressCancelDialog(void) : fProgressID(0) {}
    virtual ~ProgressCancelDialog(void) {}

    BOOL Create(bool* pCancelFlag, int dialogID, int progressID,
        CWnd* pParentWnd = NULL)
    {
        fProgressID = progressID;
        return CancelDialog::Create(pCancelFlag, dialogID, pParentWnd);
    }

    enum { kProgressResolution = 1000 };

    /*
     * Update the progress meter.
     *
     * Returns IDOK if all is well, IDCANCEL if the "cancel" button was hit.
     */
    int SetProgress(int newVal)
    {
        ASSERT(newVal >= 0 && newVal <= kProgressResolution);

        CProgressCtrl* pProgress = (CProgressCtrl*) GetDlgItem(fProgressID);
        if (pProgress != NULL) {
            /* would be nice to only set the range once */
            pProgress->SetRange(0, kProgressResolution);
            pProgress->SetPos(newVal);
        }

        if (*fpCancelFlag) {
            // dim the button to ack receipt
            CWnd* pWnd = GetDlgItem(IDCANCEL);
            pWnd->EnableWindow(FALSE);
            return IDCANCEL;
        } else
            return IDOK;
    }

private:
    DECLARE_COPY_AND_OPEQ(ProgressCancelDialog)

    BOOL OnInitDialog(void) {
        CancelDialog::OnInitDialog();

        ASSERT(fProgressID != 0);
        CProgressCtrl* pProgress = (CProgressCtrl*) GetDlgItem(fProgressID);
        ASSERT(pProgress != NULL);

        /* for some reason this doesn't work if I do it here */
        //pProgress->SetRange(0, kProgressResolution);

        pProgress->SetFocus();      // get focus off of the Cancel button
        return FALSE;               // accept our focus
    }

    // CancelDialog traps OnCancel
    // ModelessDialog traps OnCancel/OnOK

    //void PostNcDestroy(void) { delete this; }

    int     fProgressID;
};

#endif /*UTIL_PROCESSCANCELDIALOG_H*/
