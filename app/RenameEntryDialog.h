/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Rename an archive entry.
 */
#ifndef __RENAMEENTRYDIALOG__
#define __RENAMEENTRYDIALOG__

#include "GenericArchive.h"
#include "resource.h"

/*
 * Rename an entry in an archive (as opposed to renaming a file on the local
 * hard drive).
 *
 * We use the GenericArchive to verify that the name is valid before we
 * shut the dialog.
 *
 * We should probably dim the "Skip" button when there aren't any more entries
 * to rename.  This requires that the caller tell us when we're on the last
 * one.
 */
class RenameEntryDialog : public CDialog {
public:
	RenameEntryDialog(CWnd* pParentWnd = NULL) :
		CDialog(IDD_RENAME_ENTRY, pParentWnd)
	{
		fFssep = '=';
		//fNewNameLimit = 0;
		fpArchive = nil;
		fpEntry = nil;
		fCanRenameFullPath = false;
		fCanChangeFssep = false;
	}
	virtual ~RenameEntryDialog(void) {}

	void SetCanRenameFullPath(bool val) { fCanRenameFullPath = val; }
	void SetCanChangeFssep(bool val) { fCanChangeFssep = val; }

	CString		fOldName;
	char		fFssep;
	CString		fNewName;
	//int			fNewNameLimit;			// max #of chars accepted, or 0
	const GenericArchive*	fpArchive;
	const GenericEntry*		fpEntry;

protected:
	// overrides
	virtual BOOL OnInitDialog(void);
	virtual void DoDataExchange(CDataExchange* pDX);

	afx_msg void OnSkip(void);
	afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo);
	afx_msg void OnHelp(void);

private:
	//CString		fOldPath;		// pathname component, or empty if canRenFull
	CString		fOldFile;		// filename component, or full name if ^^^
	CString		fBasePath;
	CString		fFssepStr;
	bool		fCanRenameFullPath;
	bool		fCanChangeFssep;

	DECLARE_MESSAGE_MAP()
};

#endif /*__RENAMEENTRYDIALOG__*/
