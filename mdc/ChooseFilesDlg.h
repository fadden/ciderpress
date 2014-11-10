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
    //virtual bool MyDataExchange(bool saveAndValidate);
    virtual void ShiftControls(int deltaX, int deltaY);
    //virtual UINT MyOnCommand(WPARAM wParam, LPARAM lParam);

    //void OnIDHelp(void);

    //int GetButtonCheck(int id);
    //void SetButtonCheck(int id, int checkVal);

    //DECLARE_MESSAGE_MAP()
};

#endif /*MDC_CHOOSEFILESDLG_H*/
