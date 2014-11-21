/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Decide how to open the disk editor.
 */
#ifndef APP_DISKEDITOPENDIALOG_H
#define APP_DISKEDITOPENDIALOG_H

#include <afxwin.h>
#include "resource.h"

/*
 * Very simple dialog class with three buttons (plus "cancel").
 *
 * The button chosen will be returned in "fOpenWhat".
 */
class DiskEditOpenDialog : public CDialog {
public:
    typedef enum {
        kOpenUnknown = 0,
        kOpenFile,
        kOpenVolume,
        kOpenCurrent,
    } OpenWhat;

    DiskEditOpenDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_DISKEDIT_OPENWHICH, pParentWnd),
        fArchiveOpen(false), fOpenWhat(kOpenUnknown)
    {}

    // set this if the main content list has a file open
    bool fArchiveOpen;
    // return value -- which button was hit
    OpenWhat fOpenWhat;

private:
    virtual BOOL OnInitDialog(void) override;

    afx_msg void OnButtonFile(void);
    afx_msg void OnButtonVolume(void);
    afx_msg void OnButtonCurrent(void);

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_DISKEDITOPENDIALOG_H*/
