/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * File associations edit dialog.
 */
#ifndef APP_EDITASSOCDIALOG_H
#define APP_EDITASSOCDIALOG_H

#include "resource.h"

/*
 * Edit whatever associations our registry class cares about.
 */
class EditAssocDialog : public CDialog {
public:
    EditAssocDialog(CWnd* pParentWnd = NULL) :
      CDialog(IDD_ASSOCIATIONS, pParentWnd),
      fOurAssociations(NULL)
        {}
    virtual ~EditAssocDialog() {
        delete[] fOurAssociations;
    }

    // Which associations are ours.  This should be left uninitialized;
    // Setup() takes care of that.  The caller may "steal" the array
    // afterward, freeing it with delete[].
    bool*   fOurAssociations;

protected:
    // overrides
    virtual BOOL OnInitDialog(void);
    BOOL OnHelpInfo(HELPINFO* lpHelpInfo);
    void DoDataExchange(CDataExchange* pDX);

    afx_msg void OnHelp(void);

    void Setup(bool loadAssoc);

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_EDITASSOCDIALOG_H*/
