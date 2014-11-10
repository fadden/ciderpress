/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Simple dialog to offer the opportunity to open the file we just created.
 */
#ifndef APP_DONEOPENDIALOG_H
#define APP_DONEOPENDIALOG_H

#include "resource.h"

class DoneOpenDialog : public CDialog {
public:
    DoneOpenDialog(CWnd* pParentWnd = NULL) : CDialog(IDD_DONEOPEN, pParentWnd)
        {}
    virtual ~DoneOpenDialog(void) {}
};

#endif /*APP_DONEOPENDIALOG_H*/
