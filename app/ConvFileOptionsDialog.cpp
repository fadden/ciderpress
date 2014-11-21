/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#include "stdafx.h"
#include "ConvFileOptionsDialog.h"


void ConvFileOptionsDialog::DoDataExchange(CDataExchange* pDX)
{
    //DDX_Check(pDX, IDC_CONVFILE_CONVDOS, fConvDOSText);
    //DDX_Check(pDX, IDC_CONVFILE_CONVPASCAL, fConvPascalText);
    DDX_Check(pDX, IDC_CONVFILE_PRESERVEDIR, fPreserveEmptyFolders);

    UseSelectionDialog::DoDataExchange(pDX);
}
