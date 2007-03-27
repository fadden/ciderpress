/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * File associations edit dialog.
 */
#ifndef __EDITASSOCDIALOG__
#define __EDITASSOCDIALOG__

#include "resource.h"

/*
 * Edit whatever associations our registry class cares about.
 */
class EditAssocDialog : public CDialog {
public:
	EditAssocDialog(CWnd* pParentWnd = nil) :
	  CDialog(IDD_ASSOCIATIONS, pParentWnd),
	  fOurAssociations(nil)
		{}
	virtual ~EditAssocDialog() {
		delete[] fOurAssociations;
	}

	// Which associations are ours.  This should be left uninitialized;
	// Setup() takes care of that.  The caller may "steal" the array
	// afterward, freeing it with delete[].
	bool*	fOurAssociations;

protected:
	// overrides
	virtual BOOL OnInitDialog(void);
	BOOL OnHelpInfo(HELPINFO* lpHelpInfo);
	void DoDataExchange(CDataExchange* pDX);

	afx_msg void OnHelp(void);

	void Setup(bool loadAssoc);

	DECLARE_MESSAGE_MAP()
};

#endif /*__EDITASSOCDIALOG__*/