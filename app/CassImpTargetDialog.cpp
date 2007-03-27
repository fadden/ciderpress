/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Choose file name and characteristics for a file imported from an audio
 * cassette tape.
 */
#include "StdAfx.h"
#include "CassImpTargetDialog.h"
#include "GenericArchive.h"		// just want kFileTypeXXX

BEGIN_MESSAGE_MAP(CassImpTargetDialog, CDialog)
	ON_BN_CLICKED(IDC_CASSIMPTARG_BAS, OnTypeChange)
	ON_BN_CLICKED(IDC_CASSIMPTARG_INT, OnTypeChange)
	ON_BN_CLICKED(IDC_CASSIMPTARG_BIN, OnTypeChange)
	ON_EN_CHANGE(IDC_CASSIMPTARG_BINADDR, OnAddrChange)
END_MESSAGE_MAP()

/*
 * Set up the dialog.
 */
BOOL
CassImpTargetDialog::OnInitDialog(void)
{
	/* substitute our replacement edit control */
	fAddrEdit.ReplaceDlgCtrl(this, IDC_CASSIMPTARG_BINADDR);
	fAddrEdit.SetProperties(MyEdit::kCapsOnly | MyEdit::kHexOnly);

	//CWnd* pWnd;
	CEdit* pEdit;

	pEdit = (CEdit*) GetDlgItem(IDC_CASSIMPTARG_BINADDR);
	pEdit->SetLimitText(4);		// 4-digit hex value

	/* do the DDX thing, then update computed fields */
	CDialog::OnInitDialog();
	OnTypeChange();
	OnAddrChange();

	pEdit = (CEdit*) GetDlgItem(IDC_CASSIMPTARG_FILENAME);
	pEdit->SetSel(0, -1);
	pEdit->SetFocus();
	return FALSE;		// don't change the focus
}

/*
 * Copy values in and out of the dialog.
 */
void
CassImpTargetDialog::DoDataExchange(CDataExchange* pDX)
{
	DDX_Radio(pDX, IDC_CASSIMPTARG_BAS, fFileTypeIndex);
	DDX_Text(pDX, IDC_CASSIMPTARG_FILENAME, fFileName);

	if (pDX->m_bSaveAndValidate) {
		CString appName;
		appName.LoadString(IDS_MB_APP_NAME);

		if (fFileTypeIndex == kTypeBIN) {
			if (GetStartAddr() < 0) {
				MessageBox("The address field must be a valid 4-digit "
						   " hexadecimal number.",
					appName, MB_OK);
				pDX->Fail();
				return;
			}
			fStartAddr = (unsigned short) GetStartAddr();
		}
		if (fFileName.IsEmpty()) {
			MessageBox("You must enter a filename.", appName, MB_OK);
			pDX->Fail();
			return;
		}
	} else {
		CWnd* pWnd;
		CString tmpStr;

		pWnd = GetDlgItem(IDC_CASSIMPTARG_BINADDR);
		tmpStr.Format("%04X", fStartAddr);
		pWnd->SetWindowText(tmpStr);
	}
}

/*
 * They selected a different file type.  Enable or disable the address
 * entry window.
 */
void
CassImpTargetDialog::OnTypeChange(void)
{
	CButton* pButton;
	CWnd* pWnd;

	pButton = (CButton*) GetDlgItem(IDC_CASSIMPTARG_BIN);
	pWnd = GetDlgItem(IDC_CASSIMPTARG_BINADDR);

	pWnd->EnableWindow(pButton->GetCheck() == BST_CHECKED);
}

/*
 * If the user changes the address, update the "end of range" field.
 */
void
CassImpTargetDialog::OnAddrChange(void)
{
	CWnd* pWnd;
	CString tmpStr;
	long val;

	val = GetStartAddr();
	if (val < 0)
		val = 0;

	tmpStr.Format(".%04X", val + fFileLength-1);

	pWnd = GetDlgItem(IDC_CASSIMPTARG_RANGE);
	pWnd->SetWindowText(tmpStr);
}

/*
 * Get the start address (entered as a 4-digit hex value).
 *
 * Returns -1 if something was wrong with the string (e.g. empty or has
 * invalid chars).
 */
long
CassImpTargetDialog::GetStartAddr(void) const
{
	CWnd* pWnd = GetDlgItem(IDC_CASSIMPTARG_BINADDR);
	ASSERT(pWnd != nil);

	CString aux;
	pWnd->GetWindowText(aux);

	const char* str = aux;
	char* end;
	long val;

	if (str[0] == '\0') {
		WMSG0(" HEY: blank addr, returning -1\n");
		return -1;
	}
	val = strtoul(aux, &end, 16);
	if (end != str + strlen(str)) {
		WMSG1(" HEY: found some garbage in addr '%s', returning -1\n",
			(LPCTSTR) aux);
		return -1;
	}
	return val;
}

/*
 * Get the selected file type.  Call this after the modal dialog exits.
 */
long 
CassImpTargetDialog::GetFileType(void) const
{
	switch (fFileTypeIndex) {
	case kTypeBIN:	return kFileTypeBIN;
	case kTypeINT:	return kFileTypeINT;
	case kTypeBAS:	return kFileTypeBAS;
	default:
		assert(false);
		return -1;
	}
}

/*
 * Convert a ProDOS file type into a radio button enum.
 */
void
CassImpTargetDialog::SetFileType(long type)
{
	switch (type) {
	case kFileTypeBIN:	fFileTypeIndex = kTypeBIN;	break;
	case kFileTypeINT:	fFileTypeIndex = kTypeINT;	break;
	case kFileTypeBAS:	fFileTypeIndex = kTypeBAS;	break;
	default:
		assert(false);
		break;
	}
}
