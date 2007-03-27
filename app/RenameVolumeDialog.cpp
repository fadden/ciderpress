/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of RenameVolumeDialog.
 *
 * Show a tree with possible volumes and sub-volumes, and ask the user to
 * enter the desired name (or volume number).
 *
 * We need to have the tree, rather than just clicking on an entry in the file
 * list, because we want to be able to change names and volume numbers on
 * disks with no files.
 */
#include "stdafx.h"
#include "RenameVolumeDialog.h"
#include "DiskFSTree.h"
#include "DiskArchive.h"
#include "HelpTopics.h"

BEGIN_MESSAGE_MAP(RenameVolumeDialog, CDialog)
	ON_NOTIFY(TVN_SELCHANGED, IDC_RENAMEVOL_TREE, OnSelChanged)
	ON_BN_CLICKED(IDHELP, OnHelp)
	ON_WM_HELPINFO()
END_MESSAGE_MAP()

/*
 * Set up the control.
 */
BOOL
RenameVolumeDialog::OnInitDialog(void)
{
	/* do the DoDataExchange stuff */
	CDialog::OnInitDialog();

	CTreeCtrl* pTree = (CTreeCtrl*) GetDlgItem(IDC_RENAMEVOL_TREE);
	DiskImgLib::DiskFS*	pDiskFS = fpArchive->GetDiskFS();

	ASSERT(pTree != nil);

	fDiskFSTree.fIncludeSubdirs = false;
	fDiskFSTree.fExpandDepth = -1;
	if (!fDiskFSTree.BuildTree(pDiskFS, pTree)) {
		WMSG0("Tree load failed!\n");
		OnCancel();
	}

	int count = pTree->GetCount();
	WMSG1("ChooseAddTargetDialog tree has %d items\n", count);

	/* select the default text and set the focus */
	CEdit* pEdit = (CEdit*) GetDlgItem(IDC_RENAMEVOL_NEW);
	ASSERT(pEdit != nil);
	pEdit->SetSel(0, -1);
	pEdit->SetFocus();

	return FALSE;	// we set the focus
}

/*
 * Convert values.
 */
void
RenameVolumeDialog::DoDataExchange(CDataExchange* pDX)
{
	CString msg, failed;
	//DiskImgLib::DiskFS*	pDiskFS = fpArchive->GetDiskFS();

	msg = "";
	failed.LoadString(IDS_MB_APP_NAME);

	/* put fNewName last so it gets the focus after failure */
	DDX_Text(pDX, IDC_RENAMEVOL_NEW, fNewName);

	/* validate the path field */
	if (pDX->m_bSaveAndValidate) {
		/*
		 * Make sure they chose a volume that can be modified.
		 */
		CTreeCtrl* pTree = (CTreeCtrl*) GetDlgItem(IDC_RENAMEVOL_TREE);
		CString errMsg, appName;
		appName.LoadString(IDS_MB_APP_NAME);

		HTREEITEM selected;
		selected = pTree->GetSelectedItem();
		if (selected == nil) {
			errMsg = "Please select a disk to rename.";
			MessageBox(errMsg, appName, MB_OK);
			pDX->Fail();
			return;
		}

		DiskFSTree::TargetData* pTargetData;
		pTargetData = (DiskFSTree::TargetData*) pTree->GetItemData(selected);
		if (!pTargetData->selectable) {
			errMsg = "You can't rename that volume.";
			MessageBox(errMsg, appName, MB_OK);
			pDX->Fail();
			return;
		}
		ASSERT(pTargetData->kind == DiskFSTree::kTargetDiskFS);

		/*
		 * Verify that the new name is okay.  (Do this *after* checking the
		 * volume above to avoid spurious complaints about unsupported
		 * filesystems.)
		 */
		if (fNewName.IsEmpty()) {
			msg = "You must specify a new name.";
			goto fail;
		}
		msg = fpArchive->TestVolumeName(pTargetData->pDiskFS, fNewName);
		if (!msg.IsEmpty())
			goto fail;


		/*
		 * Looks good.  Fill in the answer.
		 */
		fpChosenDiskFS = pTargetData->pDiskFS;
	}

	return;

fail:
	ASSERT(!msg.IsEmpty());
	MessageBox(msg, failed, MB_OK);
	pDX->Fail();
	return;
}

/*
 * Get a notification whenever the selection changes.  Use it to stuff a
 * default value into the edit box.
 */
void
RenameVolumeDialog::OnSelChanged(NMHDR* pnmh, LRESULT* pResult)
{
	CTreeCtrl* pTree = (CTreeCtrl*) GetDlgItem(IDC_RENAMEVOL_TREE);
	HTREEITEM selected;
	CString newText;

	selected = pTree->GetSelectedItem();
	if (selected != nil) {
		DiskFSTree::TargetData* pTargetData;
		pTargetData = (DiskFSTree::TargetData*) pTree->GetItemData(selected);
		if (pTargetData->selectable) {
			newText = pTargetData->pDiskFS->GetBareVolumeName();
		} else {
			newText = "";
		}
	}
	
	CEdit* pEdit = (CEdit*) GetDlgItem(IDC_RENAMEVOL_NEW);
	ASSERT(pEdit != nil);
	pEdit->SetWindowText(newText);
	pEdit->SetSel(0, -1);

	*pResult = 0;
}

/*
 * Context help request (question mark button).
 */
BOOL
RenameVolumeDialog::OnHelpInfo(HELPINFO* lpHelpInfo)
{
	WinHelp((DWORD) lpHelpInfo->iCtrlId, HELP_CONTEXTPOPUP);
	return TRUE;	// yes, we handled it
}

/*
 * User pressed Ye Olde Helppe Button.
 */
void
RenameVolumeDialog::OnHelp(void)
{
	WinHelp(HELP_TOPIC_RENAME_VOLUME, HELP_CONTEXT);
}
