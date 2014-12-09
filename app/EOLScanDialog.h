/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * A simple dialog to display the results of an EOL scan.
 */
#ifndef APP_EOLSCANDIALOG_H
#define APP_EOLSCANDIALOG_H

#include "resource.h"

/*
 * Entire class is here.
 */
class EOLScanDialog : public CDialog {
public:
    EOLScanDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_EOLSCAN, pParentWnd)
        {}
    virtual ~EOLScanDialog(void) {}

    long    fCountChars;
    long    fCountCR;
    long    fCountLF;
    long    fCountCRLF;
    long    fCountHighASCII;

private:
    BOOL OnInitDialog(void) override;

    afx_msg void OnHelp(void) {
        MyApp::HandleHelp(this, HELP_TOPIC_EOL_SCAN);
    }

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_EOLSCANDIALOG_H*/
