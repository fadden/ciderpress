/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Options for converting a disk image to a file archive.
 */
#ifndef APP_CONFFILEOPTIONSDIALOG_H
#define APP_CONFFILEOPTIONSDIALOG_H

#include "UseSelectionDialog.h"
#include "resource.h"

/*
 * Get some options.
 */
class ConvFileOptionsDialog : public UseSelectionDialog {
public:
    ConvFileOptionsDialog(int selCount, CWnd* pParentWnd = NULL) :
        UseSelectionDialog(selCount, pParentWnd, IDD_CONVFILE_OPTS)
    {
        fPreserveEmptyFolders = FALSE;
    }
    virtual ~ConvFileOptionsDialog(void) {}

    //BOOL  fConvDOSText;
    //BOOL  fConvPascalText;
    BOOL    fPreserveEmptyFolders;

private:
    virtual void DoDataExchange(CDataExchange* pDX) override;

    //DECLARE_MESSAGE_MAP()
};

#endif /*APP_CONFFILEOPTIONSDIALOG_H*/
