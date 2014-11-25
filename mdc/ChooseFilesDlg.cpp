/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for the "choose files" dialog.
 */
#include "stdafx.h"
#include "ChooseFilesDlg.h"


void ChooseFilesDlg::ShiftControls(int deltaX, int deltaY)
{
    /*
     * These only need to be here so that the initial move puts them
     * where they belong.  Once the dialog has been created, the
     * CFileDialog will move things where they need to go.
     */
    MoveControl(this, IDC_CHOOSEFILES_STATIC1, 0, deltaY, false);
    SelectFilesDialog::ShiftControls(deltaX, deltaY);
}
