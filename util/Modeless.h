/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Trivial implementation of a modeless dialog box.
 */
#ifndef UTIL_MODELESS_H
#define UTIL_MODELESS_H

/*
 * This class must be allocated on the heap, and destroyed by calling
 * DestroyWindow().  Do not delete the object or call EndDialog().
 *
 * To use, call Create(dialogID, pParentWnd).  Make sure "visible" is set to
 * "true" in the dialog properties.
 *
 * For "progress" dialogs: immediately before creating the window with Create,
 * disable the main window with EnableWindow(FALSE).  That prevents it from
 * getting user input.  When you're done, re-enable it with
 * EnableWindow(TRUE), and then DestroyWindow on this object.  (If you do it
 * the other way, some other window will get focus, and you have to use
 * SetActiveWindow() to get it back... but that causes a UI flash.)  This
 * behavior is now implemented in ExclusiveModelessDialog.
 */
class ModelessDialog : public CDialog {
public:
    ModelessDialog(void) : fOkayToDelete(false) {}
    virtual ~ModelessDialog(void) { ASSERT(fOkayToDelete); }

    /*
     * OK button clicked.  Must override to prevent standard EndDialog
     * behavior.
     */
    virtual void OnOK(void) override {
        if (UpdateData() != FALSE)      // try the DDX/DDV stuff, if any
            DestroyWindow();
    }
    /*
     * ESC key hit or Cancel button clicked.  Must override to prevent
     * standard EndDialog behavior.
     */
    virtual void OnCancel(void) override {
        DestroyWindow();
    }

protected:
    void PostNcDestroy(void) override {
        // this may not arrive immediately
        fOkayToDelete = true;
        delete this;
    }

private:
    DECLARE_COPY_AND_OPEQ(ModelessDialog)

    bool fOkayToDelete;     // sanity check
};


/*
 * Variant of ModelessDialog that handles enabling and disabling the parent
 * window.
 */
class ExclusiveModelessDialog : public ModelessDialog {
public:
    ExclusiveModelessDialog(void) : fpParentWnd(NULL) {}
    virtual ~ExclusiveModelessDialog(void) {};

    /* disable the parent window before we're created */
    BOOL Create(int dialogID, CWnd* pParentWnd = NULL) {
        ASSERT(pParentWnd != NULL);      // else what's the point?
        if (pParentWnd != NULL) {
            fpParentWnd = pParentWnd;
            fpParentWnd->EnableWindow(FALSE);
        }
        return ModelessDialog::Create(dialogID, pParentWnd);
    }

    /* enable the parent window before we're destroyed */
    virtual BOOL DestroyWindow(void) override {
        if (fpParentWnd != NULL)
            fpParentWnd->EnableWindow(TRUE);
        return ModelessDialog::DestroyWindow();
    }

private:
    DECLARE_COPY_AND_OPEQ(ExclusiveModelessDialog)

    CWnd*   fpParentWnd;
};

/*
    From DLGCORE.CPP line 516:

    // disable parent (before creating dialog)
    HWND hWndParent = PreModal();   // ATM: finds parent of modal dlg
    AfxUnhookWindowCreate();
    BOOL bEnableParent = FALSE;
    if (hWndParent != NULL && ::IsWindowEnabled(hWndParent))
    {
        ::EnableWindow(hWndParent, FALSE);
        bEnableParent = TRUE;
    }

[...]

    if (bEnableParent)
        ::EnableWindow(hWndParent, TRUE);
    if (hWndParent != NULL && ::GetActiveWindow() == m_hWnd)
        ::SetActiveWindow(hWndParent);

    // destroy modal window
    DestroyWindow();
    PostModal();

*/

#endif /*UTIL_MODELESS_H*/
