/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Paste Special dialog.
 */
#ifndef APP_PASTESPECIALDIALOG_H
#define APP_PASTESPECIALDIALOG_H

#include "resource.h"

/*
 * Simple dialog with a radio button.
 */
class PasteSpecialDialog : public CDialog {
public:
    PasteSpecialDialog(CWnd* pParentWnd = NULL) :
      CDialog(IDD_PASTE_SPECIAL, pParentWnd),
      fPasteHow(kPastePaths)
      {}
     virtual ~PasteSpecialDialog() {}

     /* right now this is boolean, but that may change */
     /* (e.g. "paste clipboard contents into new text file") */
     enum {
        kPasteUnknown = 0,
        kPastePaths,
        kPasteNoPaths,
     };
     int    fPasteHow;

protected:
    //virtual BOOL OnInitDialog(void);
    void DoDataExchange(CDataExchange* pDX) override;

    //DECLARE_MESSAGE_MAP()
};

#endif /*APP_PASTESPECIALDIALOG_H*/
