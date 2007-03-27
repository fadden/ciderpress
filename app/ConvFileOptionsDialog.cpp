/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for ConvFileOptionsDialog.
 */
#include "stdafx.h"
#include "ConvFileOptionsDialog.h"
//#include "NufxArchive.h"

#if 0
/*
 * Set up our modified version of the "use selection" dialog.
 */
BOOL
ConvFileOptionsDialog::OnInitDialog(void)
{
	return UseSelectionDialog::OnInitDialog();
}
#endif

/*
 * Convert values.
 */
void
ConvFileOptionsDialog::DoDataExchange(CDataExchange* pDX)
{
	//DDX_Check(pDX, IDC_CONVFILE_CONVDOS, fConvDOSText);
	//DDX_Check(pDX, IDC_CONVFILE_CONVPASCAL, fConvPascalText);
	DDX_Check(pDX, IDC_CONVFILE_PRESERVEDIR, fPreserveEmptyFolders);

	UseSelectionDialog::DoDataExchange(pDX);
}
