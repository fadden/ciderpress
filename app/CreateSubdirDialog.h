/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Create a subdirectory (e.g. on a ProDOS disk image).
 */
#ifndef __CREATESUBDIRDIALOG__
#define __CREATESUBDIRDIALOG__

#include "GenericArchive.h"
#include "resource.h"

/*
 * Get the name of the subdirectory to create.
 */
class CreateSubdirDialog : public CDialog {
public:
	CreateSubdirDialog(CWnd* pParentWnd = NULL) :
		CDialog(IDD_CREATE_SUBDIR, pParentWnd)
	{
		fpArchive = nil;
		fpParentEntry = nil;
	}
	virtual ~CreateSubdirDialog(void) {}

	CString		fBasePath;		// where subdir will be created
	CString		fNewName;
	const GenericArchive*	fpArchive;
	const GenericEntry*		fpParentEntry;

protected:
	// overrides
	virtual BOOL OnInitDialog(void);
	virtual void DoDataExchange(CDataExchange* pDX);

	afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo);

private:

	DECLARE_MESSAGE_MAP()
};

#endif /*__CREATESUBDIRDIALOG__*/
