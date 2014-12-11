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
#ifdef CAN_UPDATE_FILE_ASSOC
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
    virtual BOOL OnInitDialog(void) override;
    void DoDataExchange(CDataExchange* pDX) override;

    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo) {
        return MyApp::HandleHelpInfo(lpHelpInfo);
    }
    afx_msg void OnHelp(void) {
        MyApp::HandleHelp(this, HELP_TOPIC_EDIT_ASSOC);
    }

    /*
     * Load the list view control.
     *
     * This list isn't sorted, so we don't need to stuff anything into lParam to
     * keep the list and source data tied.
     *
     * If "loadAssoc" is true, we also populate the fOurAssocations table.
     */
    void Setup(bool loadAssoc);

    DECLARE_MESSAGE_MAP()
};

#endif
#endif /*APP_EDITASSOCDIALOG_H*/
