/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * TwoImg properties edit dialog.
 */
#ifndef APP_TWOIMGPROPSDIALOG_H
#define APP_TWOIMGPROPSDIALOG_H

#include "resource.h"
#include "../diskimg/TwoImg.h"


/*
 * Dialog with a bunch of controls that map to fields in the TwoImg file
 * header.  We want to keep the "Save" button ("OK") dimmed until we have
 * something to write.
 */
class TwoImgPropsDialog : public CDialog {
public:
    TwoImgPropsDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_TWOIMG_PROPS, pParentWnd),
        fpHeader(NULL), fReadOnly(false)
        {}


    void Setup(TwoImgHeader* pHeader, bool readOnly) {
        fpHeader = pHeader;
        fReadOnly = readOnly;
    }

protected:
    // overrides
    virtual BOOL OnInitDialog(void) override;
    virtual void DoDataExchange(CDataExchange* pDX) override;

    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo) {
        return MyApp::HandleHelpInfo(lpHelpInfo);
    }

    /*
     * If they changed anything, enable the "save" button.
     */
    afx_msg void OnChange(void);

    TwoImgHeader*   fpHeader;
    bool            fReadOnly;
    //bool          fModified;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_TWOIMGPROPSDIALOG_H*/
