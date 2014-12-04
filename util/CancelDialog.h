/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Display a modeless dialog with a "cancel" button.
 *
 * The user of this class must define a dialog with at least an IDCANCEL
 * button.
 */
#ifndef UTIL_CANCELDIALOG_H
#define UTIL_CANCELDIALOG_H

/*
 * This class must be allocated on the heap.
 *
 * Create with the dialog ID, parent window, and a pointer to a boolean that
 * will be set to "true" if the user hits "cancel".
 *
 * [This should probably derive from ExclusiveModelessDialog, now that we have
 * that available.  Need to go through and clean up all CancelDialog clients.]
 */
class CancelDialog : public ModelessDialog {
public:
    CancelDialog(void) {}
    virtual ~CancelDialog(void) {}

    BOOL Create(bool* pCancelFlag, int dialogID, CWnd* pParentWnd = NULL) {
        fpCancelFlag = pCancelFlag;
        *fpCancelFlag = false;
        return ModelessDialog::Create(dialogID, pParentWnd);
    }

protected:
    bool*   fpCancelFlag;

private:
    DECLARE_COPY_AND_OPEQ(CancelDialog)

    /* override Cancel button to just raise the flag */
    virtual void OnCancel(void) override {
        /* "cancel" button or escape hit */
        *fpCancelFlag = true;
    }
    //void PostNcDestroy(void) override { delete this; }

};

#endif /*UTIL_CANCELDIALOG_H*/
