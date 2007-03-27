/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Dialog for choosing a directory.
 */
#ifndef __CHOOSEDIRDIALOG__
#define __CHOOSEDIRDIALOG__

#include "../util/UtilLib.h"
#include "resource.h"

/*
 * Choose a directory.  This is distinctly different from what the standard
 * "Open" and "Save As" dialogs do, because those want to choose normal files
 * only, while this wants to select a folder.
 */
class ChooseDirDialog : public CDialog {
public:
	ChooseDirDialog(CWnd* pParent = NULL, int dialogID = IDD_CHOOSEDIR) :
		CDialog(dialogID, pParent)
	{
		fPathName = "";
	}
	virtual ~ChooseDirDialog(void) {}

	const char* GetPathName(void) const { return fPathName; }

	// set the pathname; when DoModal is called this will tunnel in
	void SetPathName(const char* str) { fPathName = str; }

protected:
	virtual BOOL OnInitDialog(void);
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	afx_msg void OnSelChanged(NMHDR* pnmh, LRESULT* pResult);
	afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo);
	afx_msg void OnExpandTree(void);
	afx_msg void OnNewFolder(void);
	afx_msg void OnHelp(void);

private:
	CString			fPathName;

	ShellTree		fShellTree;
	MyBitmapButton	fNewFolderButton;

	DECLARE_MESSAGE_MAP()
};

#endif /*__CHOOSEDIRDIALOG__*/