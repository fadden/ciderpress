/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Acknowledge and clarify a request to do something, e.g. delete files.
 */
#ifndef APP_USESELECTIONDIALOG_H
#define APP_USESELECTIONDIALOG_H

#include "resource.h"

/*
 * Straightforward confirmation.
 */
class UseSelectionDialog : public CDialog {
public:
    UseSelectionDialog(int selCount, CWnd* pParentWnd = NULL, int rsrcID = IDD_USE_SELECTION) :
        CDialog(rsrcID, pParentWnd), fSelectedCount(selCount)
    {
        // init values; these should be overridden before DoModal
        fFilesToAction = 0;
    }
    virtual ~UseSelectionDialog(void) {}

    // set up dialog parameters; must be called before DoModal
    void Setup(int titleID, int okLabelID, int countID, int countsID,
        int allID)
    {
        fTitleID = titleID;
        fOkLabelID = okLabelID;
        fSelCountID = countID;
        fSelCountsID = countsID;
        fAllID = allID;
    }

    enum { kActionSelection = 0, kActionAll = 1 };
    int     fFilesToAction;

protected:
    virtual BOOL OnInitDialog(void) override;
    virtual void DoDataExchange(CDataExchange* pDX) override;

    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo) {
        return MyApp::HandleHelpInfo(lpHelpInfo);
    }

private:
    int     fSelectedCount;

    /* dialog parameters */
    int     fTitleID;
    int     fOkLabelID;
    int     fSelCountID;
    int     fSelCountsID;
    int     fAllID;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_USESELECTIONDIALOG_H*/
