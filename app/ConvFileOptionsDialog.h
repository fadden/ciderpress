/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Options for converting a disk image to a file archive.
 */
#ifndef __CONVFILE_OPTIONS_DIALOG__
#define __CONVFILE_OPTIONS_DIALOG__

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

	//BOOL	fConvDOSText;
	//BOOL	fConvPascalText;
	BOOL	fPreserveEmptyFolders;

private:
	//virtual BOOL OnInitDialog(void);
	virtual void DoDataExchange(CDataExchange* pDX);

	//DECLARE_MESSAGE_MAP()
};

#endif /*__CONVFILE_OPTIONS_DIALOG__*/