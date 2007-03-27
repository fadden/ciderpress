/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Functions for the ChooseAddTarget dialog box.
 */
#include "StdAfx.h"
#include "ChooseAddTargetDialog.h"
#include "HelpTopics.h"
#include "DiskFSTree.h"

using namespace DiskImgLib;

BEGIN_MESSAGE_MAP(ChooseAddTargetDialog, CDialog)
	ON_COMMAND(IDHELP, OnHelp)
END_MESSAGE_MAP()

/*
 * Initialize the dialog box.  This requires scanning the provided disk
 * archive.
 */
BOOL
ChooseAddTargetDialog::OnInitDialog(void)
{
	CDialog::OnInitDialog();

	CTreeCtrl* pTree = (CTreeCtrl*) GetDlgItem(IDC_ADD_TARGET_TREE);

	ASSERT(fpDiskFS != nil);
	ASSERT(pTree != nil);

	fDiskFSTree.fIncludeSubdirs = true;
	fDiskFSTree.fExpandDepth = -1;
	if (!fDiskFSTree.BuildTree(fpDiskFS, pTree)) {
		WMSG0("Tree load failed!\n");
		OnCancel();
	}

	int count = pTree->GetCount();
	WMSG1("ChooseAddTargetDialog tree has %d items\n", count);
	if (count <= 1) {
		WMSG0(" Skipping out of target selection\n");
		// adding to root volume of the sole DiskFS
		fpChosenDiskFS = fpDiskFS;
		ASSERT(fpChosenSubdir == nil);
		OnOK();
	}

	return TRUE;
}

/*
 * Not much to do on the way in.  On the way out, make sure that they've
 * selected something acceptable, and copy the values to an easily
 * accessible location.
 */
void
ChooseAddTargetDialog::DoDataExchange(CDataExchange* pDX)
{
	if (pDX->m_bSaveAndValidate) {
		CTreeCtrl* pTree = (CTreeCtrl*) GetDlgItem(IDC_ADD_TARGET_TREE);
		CString errMsg, appName;
		appName.LoadString(IDS_MB_APP_NAME);

		/* shortcut for simple disk images */
		if (pTree->GetCount() == 1 && fpChosenDiskFS != nil)
			return;

		HTREEITEM selected;
		selected = pTree->GetSelectedItem();
		if (selected == nil) {
			errMsg = "Please select a disk or subdirectory to add files to.";
			MessageBox(errMsg, appName, MB_OK);
			pDX->Fail();
			return;
		}

		DiskFSTree::TargetData* pTargetData;
		pTargetData = (DiskFSTree::TargetData*) pTree->GetItemData(selected);
		if (!pTargetData->selectable) {
			errMsg = "You can't add files there.";
			MessageBox(errMsg, appName, MB_OK);
			pDX->Fail();
			return;
		}

		fpChosenDiskFS = pTargetData->pDiskFS;
		fpChosenSubdir = pTargetData->pFile;
	}
}


/*
 * User pressed the "Help" button.
 */
void
ChooseAddTargetDialog::OnHelp(void)
{
	WinHelp(HELP_TOPIC_CHOOSE_TARGET, HELP_CONTEXT);
}
