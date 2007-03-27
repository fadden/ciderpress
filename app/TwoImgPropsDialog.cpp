/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * TwoImg properties editor.
 */
#include "StdAfx.h"
#include "TwoImgPropsDialog.h"

BEGIN_MESSAGE_MAP(TwoImgPropsDialog, CDialog)
	ON_BN_CLICKED(IDC_TWOIMG_LOCKED, OnChange)
	ON_BN_CLICKED(IDC_TWOIMG_DOSVOLSET, OnChange)
	ON_EN_CHANGE(IDC_TWOIMG_DOSVOLNUM, OnChange)
	ON_EN_CHANGE(IDC_TWOIMG_COMMENT, OnChange)
	ON_WM_HELPINFO()
END_MESSAGE_MAP()


/*
 * Initialize the dialog from fpHeader.
 */
BOOL
TwoImgPropsDialog::OnInitDialog(void)
{
	CWnd* pWnd;
	CEdit* pEdit;
	CString tmpStr;

	ASSERT(fpHeader != nil);

	/*
	 * Set up the static fields.
	 */
	pWnd = GetDlgItem(IDC_TWOIMG_CREATOR);
	tmpStr.Format("'%s'", fpHeader->GetCreatorStr());
	pWnd->SetWindowText(tmpStr);

	pWnd = GetDlgItem(IDC_TWOIMG_VERSION);
	tmpStr.Format("%d", fpHeader->fVersion);
	pWnd->SetWindowText(tmpStr);

	pWnd = GetDlgItem(IDC_TWOIMG_FORMAT);
	switch (fpHeader->fImageFormat) {
	case TwoImgHeader::kImageFormatDOS:		tmpStr = "DOS order sectors";		break;
	case TwoImgHeader::kImageFormatProDOS:	tmpStr = "ProDOS order sectors";	break;
	case TwoImgHeader::kImageFormatNibble:	tmpStr = "Raw nibbles";				break;
	default:								tmpStr = "Unknown";					break;
	}
	pWnd->SetWindowText(tmpStr);

	pWnd = GetDlgItem(IDC_TWOIMG_BLOCKS);
	tmpStr.Format("%d", fpHeader->fNumBlocks);
	pWnd->SetWindowText(tmpStr);

	/*
	 * Restrict the edit field.
	 */
	pEdit = (CEdit*) GetDlgItem(IDC_TWOIMG_DOSVOLNUM);
	pEdit->LimitText(3);		// 1-254

	/*
	 * Disable the "Save" button.
	 */
	pWnd = GetDlgItem(IDOK);
	pWnd->EnableWindow(FALSE);

	/* for read-only mode, all buttons are disabled */
	if (fReadOnly) {
		GetDlgItem(IDC_TWOIMG_LOCKED)->EnableWindow(FALSE);
		GetDlgItem(IDC_TWOIMG_DOSVOLSET)->EnableWindow(FALSE);
		GetDlgItem(IDC_TWOIMG_COMMENT)->EnableWindow(FALSE);
		GetDlgItem(IDC_TWOIMG_DOSVOLNUM)->EnableWindow(FALSE);

		GetWindowText(tmpStr);
		tmpStr += " (read-only)";
		SetWindowText(tmpStr);
	}

	return CDialog::OnInitDialog();
}

/*
 * Do the data exchange, and set values in the header.
 */
void
TwoImgPropsDialog::DoDataExchange(CDataExchange* pDX)
{
	BOOL locked, dosVolSet;
	CString comment;
	int dosVolNum;

	if (pDX->m_bSaveAndValidate) {
		DDX_Check(pDX, IDC_TWOIMG_LOCKED, locked);
		DDX_Check(pDX, IDC_TWOIMG_DOSVOLSET, dosVolSet);
		DDX_Text(pDX, IDC_TWOIMG_COMMENT, comment);
		DDX_Text(pDX, IDC_TWOIMG_DOSVOLNUM, dosVolNum);

		WMSG1("GOT dosVolNum = %d\n", dosVolNum);

		fpHeader->fFlags &= ~(TwoImgHeader::kFlagLocked);
		if (locked)
			fpHeader->fFlags |= TwoImgHeader::kFlagLocked;

		fpHeader->fFlags &= ~(TwoImgHeader::kDOSVolumeMask);
		fpHeader->fFlags &= ~(TwoImgHeader::kDOSVolumeSet);
		if (dosVolSet) {
			fpHeader->fFlags |= TwoImgHeader::kDOSVolumeSet;
			fpHeader->fFlags |= (dosVolNum & TwoImgHeader::kDOSVolumeMask);

			CString appStr, errMsg;
			if (dosVolNum < 1 || dosVolNum > 254) {
				appStr.LoadString(IDS_MB_APP_NAME);
				errMsg.LoadString(IDS_VALID_VOLNAME_DOS);
				MessageBox(errMsg, appStr, MB_OK);
				pDX->Fail();
			} else {
				fpHeader->SetDOSVolumeNum(dosVolNum);
			}
		}


		if (!comment.IsEmpty())
			fpHeader->SetComment(comment);
		else
			fpHeader->SetComment(nil);
	} else {
		CWnd* pWnd;

		locked = (fpHeader->fFlags & TwoImgHeader::kFlagLocked) != 0;
		dosVolSet = (fpHeader->fFlags & TwoImgHeader::kDOSVolumeSet) != 0;
		comment = fpHeader->GetComment();
		if (dosVolSet)
			dosVolNum = fpHeader->GetDOSVolumeNum();
		else
			dosVolNum = TwoImgHeader::kDefaultVolumeNum;

		DDX_Check(pDX, IDC_TWOIMG_LOCKED, locked);
		DDX_Check(pDX, IDC_TWOIMG_DOSVOLSET, dosVolSet);
		DDX_Text(pDX, IDC_TWOIMG_COMMENT, comment);
		DDX_Text(pDX, IDC_TWOIMG_DOSVOLNUM, dosVolNum);

		/* set initial state of dos volume number edit field */
		if (!fReadOnly) {
			pWnd = GetDlgItem(IDC_TWOIMG_DOSVOLNUM);
			pWnd->EnableWindow(dosVolSet);
		}
	}
}

/*
 * If they changed anything, enable the "save" button.
 */
void
TwoImgPropsDialog::OnChange(void)
{
	CButton* pButton;
	UINT checked;

	ASSERT(!fReadOnly);

	GetDlgItem(IDOK)->EnableWindow(TRUE);

	pButton = (CButton*) GetDlgItem(IDC_TWOIMG_DOSVOLSET);
	checked = pButton->GetCheck();
	GetDlgItem(IDC_TWOIMG_DOSVOLNUM)->EnableWindow(checked == BST_CHECKED);
}

/*
 * Context help request (question mark button).
 */
BOOL
TwoImgPropsDialog::OnHelpInfo(HELPINFO* lpHelpInfo)
{
	WinHelp((DWORD) lpHelpInfo->iCtrlId, HELP_CONTEXTPOPUP);
	return TRUE;	// yes, we handled it
}
