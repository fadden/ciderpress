/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Choose files and directories.
 */
#ifndef MDC_CHOOSEFILESDLG_H
#define MDC_CHOOSEFILESDLG_H

#include "../util/UtilLib.h"
#include "resource.h"

class ChooseFilesDlg : public SelectFilesDialog {
public:
    ChooseFilesDlg(CWnd* pParentWnd = NULL) :
        SelectFilesDialog(L"IDD_CHOOSE_FILES", pParentWnd)
    {
        SetWindowTitle(L"Choose Files...");

        fAcceptButtonID = IDC_SELECT_ACCEPT;
    }
    virtual ~ChooseFilesDlg(void) {}

private:
    /*
     * Override base class version so we can move our stuff around.
     *
     * It's important that the base class be called last, because it calls
     * Invalidate to redraw the dialog.
     */
    virtual void ShiftControls(int deltaX, int deltaY) override;

    //DECLARE_MESSAGE_MAP()
};

#endif /*MDC_CHOOSEFILESDLG_H*/
