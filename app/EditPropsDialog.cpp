/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for file properties edit dialog.
 */
#include "StdAfx.h"
#include "EditPropsDialog.h"
#include "FileNameConv.h"
#include "HelpTopics.h"

using namespace DiskImgLib;

BEGIN_MESSAGE_MAP(EditPropsDialog, CDialog)
	ON_BN_CLICKED(IDC_PROPS_ACCESS_W, UpdateSimpleAccess)
	ON_BN_CLICKED(IDC_PROPS_HFS_MODE, UpdateHFSMode)
	ON_CBN_SELCHANGE(IDC_PROPS_FILETYPE, OnTypeChange)
	ON_EN_CHANGE(IDC_PROPS_AUXTYPE, OnTypeChange)
	ON_EN_CHANGE(IDC_PROPS_HFS_FILETYPE, OnHFSTypeChange)
	ON_EN_CHANGE(IDC_PROPS_HFS_AUXTYPE, OnHFSTypeChange)
	ON_WM_HELPINFO()
	ON_COMMAND(IDHELP, OnHelp)
END_MESSAGE_MAP()


/*
 * Initialize fProps from the stuff in pEntry.
 */
void
EditPropsDialog::InitProps(GenericEntry* pEntry)
{
	fPathName = pEntry->GetPathName();
	fProps.fileType = pEntry->GetFileType();
	fProps.auxType = pEntry->GetAuxType();
	fProps.access = pEntry->GetAccess();
	fProps.createWhen = pEntry->GetCreateWhen();
	fProps.modWhen = pEntry->GetModWhen();

	if (!pEntry->GetFeatureFlag(GenericEntry::kFeatureCanChangeType))
		fAllowedTypes = kAllowedNone;
	else if (pEntry->GetFeatureFlag(GenericEntry::kFeaturePascalTypes))
		fAllowedTypes = kAllowedPascal;
	else if (pEntry->GetFeatureFlag(GenericEntry::kFeatureDOSTypes))
		fAllowedTypes = kAllowedDOS;
	else if (pEntry->GetFeatureFlag(GenericEntry::kFeatureHFSTypes))
		fAllowedTypes = kAllowedHFS;	// for HFS disks and ShrinkIt archives
	else
		fAllowedTypes = kAllowedProDOS;
	if (!pEntry->GetFeatureFlag(GenericEntry::kFeatureHasFullAccess)) {
		if (pEntry->GetFeatureFlag(GenericEntry::kFeatureHasSimpleAccess))
			fSimpleAccess = true;
		else
			fNoChangeAccess = true;
	}
	if (pEntry->GetFeatureFlag(GenericEntry::kFeatureHasInvisibleFlag))
		fAllowInvis = true;
}


/*
 * Set up the control.  We need to load the drop list with the file type
 * info, and configure any controls that aren't set by DoDataExchange.
 *
 * If this is a disk archive, we might want to make the aux type read-only,
 * though this would provide a way for users to fix badly-formed archives.
 */
BOOL
EditPropsDialog::OnInitDialog(void)
{
	static const int kPascalTypes[] = {
		0x00 /*NON*/,	0x01 /*BAD*/,	0x02 /*PCD*/,	0x03 /*PTX*/,
		0xf3 /*$F3*/,	0x05 /*PDA*/,	0xf4 /*$F4*/,	0x08 /*FOT*/,
		0xf5 /*$f5*/
	};
	static const int kDOSTypes[] = {
		0x04 /*TXT*/,	0x06 /*BIN*/,	0xf2 /*$F2*/,	0xf3 /*$F3*/,
		0xf4 /*$F4*/,	0xfa /*INT*/,	0xfc /*BAS*/,	0xfe /*REL*/
	};
	CComboBox* pCombo;
	CWnd* pWnd;
	int comboIdx;

	pCombo = (CComboBox*) GetDlgItem(IDC_PROPS_FILETYPE);
	ASSERT(pCombo != nil);

	pCombo->InitStorage(256, 256 * 8);

	for (int type = 0; type < 256; type++) {
		const char* str;
		char buf[10];

		if (fAllowedTypes == kAllowedPascal) {
			/* not the most efficient way, but it'll do */
			for (int j = 0; j < NELEM(kPascalTypes); j++) {
				if (kPascalTypes[j] == type)
					break;
			}
			if (j == NELEM(kPascalTypes))
				continue;
		} else if (fAllowedTypes == kAllowedDOS) {
			for (int j = 0; j < NELEM(kDOSTypes); j++) {
				if (kDOSTypes[j] == type)
					break;
			}
			if (j == NELEM(kDOSTypes))
				continue;
		}

		str = PathProposal::FileTypeString(type);
		if (str[0] == '$')
			sprintf(buf, "??? $%02X", type);
		else
			sprintf(buf, "%s $%02X", str, type);
		comboIdx = pCombo->AddString(buf);
		pCombo->SetItemData(comboIdx, type);

		if ((int) fProps.fileType == type)
			pCombo->SetCurSel(comboIdx);
	}
	if (fProps.fileType >= 256) {
		if (fAllowedTypes == kAllowedHFS) {
			pCombo->SetCurSel(0);
		} else {
			// unexpected -- bogus data out of DiskFS?
			comboIdx = pCombo->AddString("???");
			pCombo->SetCurSel(comboIdx);
			pCombo->SetItemData(comboIdx, 256);
		}
	}

	CString dateStr;
	pWnd = GetDlgItem(IDC_PROPS_CREATEWHEN);
	ASSERT(pWnd != nil);
	FormatDate(fProps.createWhen, &dateStr);
	pWnd->SetWindowText(dateStr);

	pWnd = GetDlgItem(IDC_PROPS_MODWHEN);
	ASSERT(pWnd != nil);
	FormatDate(fProps.modWhen, &dateStr);
	pWnd->SetWindowText(dateStr);
	//WMSG2("USING DATE '%s' from 0x%08lx\n", dateStr, fProps.modWhen);

	CEdit* pEdit = (CEdit*) GetDlgItem(IDC_PROPS_AUXTYPE);
	ASSERT(pEdit != nil);
	pEdit->SetLimitText(4);		// max len of aux type str
	pEdit = (CEdit*) GetDlgItem(IDC_PROPS_HFS_FILETYPE);
	pEdit->SetLimitText(4);
	pEdit = (CEdit*) GetDlgItem(IDC_PROPS_HFS_AUXTYPE);
	pEdit->SetLimitText(4);

	if (fReadOnly || fAllowedTypes == kAllowedNone) {
		pWnd = GetDlgItem(IDC_PROPS_FILETYPE);
		pWnd->EnableWindow(FALSE);
		pWnd = GetDlgItem(IDC_PROPS_AUXTYPE);
		pWnd->EnableWindow(FALSE);
	} else if (fAllowedTypes == kAllowedPascal) {
		pWnd = GetDlgItem(IDC_PROPS_AUXTYPE);
		pWnd->EnableWindow(FALSE);
	}
	if (fReadOnly || fSimpleAccess || fNoChangeAccess) {
		pWnd = GetDlgItem(IDC_PROPS_ACCESS_R);
		pWnd->EnableWindow(FALSE);
		pWnd = GetDlgItem(IDC_PROPS_ACCESS_B);
		pWnd->EnableWindow(FALSE);
		pWnd = GetDlgItem(IDC_PROPS_ACCESS_N);
		pWnd->EnableWindow(FALSE);
		pWnd = GetDlgItem(IDC_PROPS_ACCESS_D);
		pWnd->EnableWindow(FALSE);
	}
	if (fReadOnly || !fAllowInvis) {
		pWnd = GetDlgItem(IDC_PROPS_ACCESS_I);
		pWnd->EnableWindow(FALSE);
	}
	if (fReadOnly || fNoChangeAccess) {
		pWnd = GetDlgItem(IDC_PROPS_ACCESS_W);
		pWnd->EnableWindow(FALSE);
	}
	if (fReadOnly) {
		pWnd = GetDlgItem(IDOK);
		pWnd->EnableWindow(FALSE);

		CString title;
		GetWindowText(/*ref*/ title);
		title = title + " (read only)";
		SetWindowText(title);
	}

	if (fAllowedTypes != kAllowedHFS) {
		CButton* pButton = (CButton*) GetDlgItem(IDC_PROPS_HFS_MODE);
		pButton->EnableWindow(FALSE);
	}

	return CDialog::OnInitDialog();
}

/*
 * Convert values.
 */
void
EditPropsDialog::DoDataExchange(CDataExchange* pDX)
{
	int fileTypeIdx;
	BOOL accessR, accessW, accessI, accessB, accessN, accessD;
	CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_PROPS_FILETYPE);

	if (pDX->m_bSaveAndValidate) {
		CString appName;
		CButton *pButton;
		bool typeChanged = false;

		appName.LoadString(IDS_MB_APP_NAME);

		pButton = (CButton*) GetDlgItem(IDC_PROPS_HFS_MODE);
		if (pButton->GetCheck() == BST_CHECKED) {
			/* HFS mode */
			CString type, creator;
			DDX_Text(pDX, IDC_PROPS_HFS_FILETYPE, type);
			DDX_Text(pDX, IDC_PROPS_HFS_AUXTYPE, creator);
			if (type.GetLength() != 4 || creator.GetLength() != 4) {
				MessageBox("The file and creator types must be exactly"
						   " 4 characters each.",
					appName, MB_OK);
				pDX->Fail();
				return;
			}
			fProps.fileType = ((unsigned char) type[0]) << 24 |
							  ((unsigned char) type[1]) << 16 |
							  ((unsigned char) type[2]) << 8 |
							  ((unsigned char) type[3]);
			fProps.auxType  = ((unsigned char) creator[0]) << 24 |
							  ((unsigned char) creator[1]) << 16 |
							  ((unsigned char) creator[2]) << 8 |
							  ((unsigned char) creator[3]);
		} else {
			/* ProDOS mode */
			if (GetAuxType() < 0) {
				MessageBox("The AuxType field must be a valid 4-digit"
						   " hexadecimal number.",
					appName, MB_OK);
				pDX->Fail();
				return;
			}
			fProps.auxType = GetAuxType();

			/* pull the file type out, but don't disturb >= 256 */
			DDX_CBIndex(pDX, IDC_PROPS_FILETYPE, fileTypeIdx);
			if (fileTypeIdx != 256) {
				unsigned long oldType = fProps.fileType;
				fProps.fileType = pCombo->GetItemData(fileTypeIdx);
				if (fProps.fileType != oldType)
					typeChanged = true;
			}
		}

		DDX_Check(pDX, IDC_PROPS_ACCESS_R, accessR);
		DDX_Check(pDX, IDC_PROPS_ACCESS_W, accessW);
		DDX_Check(pDX, IDC_PROPS_ACCESS_I, accessI);
		DDX_Check(pDX, IDC_PROPS_ACCESS_B, accessB);
		DDX_Check(pDX, IDC_PROPS_ACCESS_N, accessN);
		DDX_Check(pDX, IDC_PROPS_ACCESS_D, accessD);
		fProps.access = (accessR ? GenericEntry::kAccessRead : 0) |
						(accessW ? GenericEntry::kAccessWrite : 0) |
						(accessI ? GenericEntry::kAccessInvisible : 0) |
						(accessB ? GenericEntry::kAccessBackup : 0) |
						(accessN ? GenericEntry::kAccessRename : 0) |
						(accessD ? GenericEntry::kAccessDelete : 0);

		if (fAllowedTypes == kAllowedDOS && typeChanged &&
			(fProps.fileType == kFileTypeBIN ||
			 fProps.fileType == kFileTypeINT ||
			 fProps.fileType == kFileTypeBAS))
		{
			CString msg;
			int result;

			msg.LoadString(IDS_PROPS_DOS_TYPE_CHANGE);
			result = MessageBox(msg, appName, MB_ICONQUESTION|MB_OKCANCEL);
			if (result != IDOK) {
				pDX->Fail();
				return;
			}
		}
	} else {
		accessR = (fProps.access & GenericEntry::kAccessRead) != 0;
		accessW = (fProps.access & GenericEntry::kAccessWrite) != 0;
		accessI = (fProps.access & GenericEntry::kAccessInvisible) != 0;
		accessB = (fProps.access & GenericEntry::kAccessBackup) != 0;
		accessN = (fProps.access & GenericEntry::kAccessRename) != 0;
		accessD = (fProps.access & GenericEntry::kAccessDelete) != 0;
		DDX_Check(pDX, IDC_PROPS_ACCESS_R, accessR);
		DDX_Check(pDX, IDC_PROPS_ACCESS_W, accessW);
		DDX_Check(pDX, IDC_PROPS_ACCESS_I, accessI);
		DDX_Check(pDX, IDC_PROPS_ACCESS_B, accessB);
		DDX_Check(pDX, IDC_PROPS_ACCESS_N, accessN);
		DDX_Check(pDX, IDC_PROPS_ACCESS_D, accessD);

		if (fAllowedTypes == kAllowedHFS &&
			(fProps.fileType > 0xff || fProps.auxType > 0xffff))
		{
			char type[5], creator[5];

			type[0] = (unsigned char) (fProps.fileType >> 24);
			type[1] = (unsigned char) (fProps.fileType >> 16);
			type[2] = (unsigned char) (fProps.fileType >> 8);
			type[3] = (unsigned char)  fProps.fileType;
			type[4] = '\0';
			creator[0] = (unsigned char) (fProps.auxType >> 24);
			creator[1] = (unsigned char) (fProps.auxType >> 16);
			creator[2] = (unsigned char) (fProps.auxType >> 8);
			creator[3] = (unsigned char)  fProps.auxType;
			creator[4] = '\0';

			CString tmpStr;
			tmpStr = type;
			DDX_Text(pDX, IDC_PROPS_HFS_FILETYPE, tmpStr);
			tmpStr = creator;
			DDX_Text(pDX, IDC_PROPS_HFS_AUXTYPE, tmpStr);
			tmpStr = "0000";
			DDX_Text(pDX, IDC_PROPS_AUXTYPE, tmpStr);

			CButton* pButton = (CButton*) GetDlgItem(IDC_PROPS_HFS_MODE);
			pButton->SetCheck(BST_CHECKED);
		} else {
			//fileTypeIdx = fProps.fileType;
			//if (fileTypeIdx > 256)
			//	fileTypeIdx = 256;
			//DDX_CBIndex(pDX, IDC_PROPS_FILETYPE, fileTypeIdx);

			/* write the aux type as a hex string */
			fAuxType.Format("%04X", fProps.auxType);
			DDX_Text(pDX, IDC_PROPS_AUXTYPE, fAuxType);
		}
		OnTypeChange();			// set the description field
		UpdateHFSMode();		// set up fields
		UpdateSimpleAccess();	// coordinate N/D with W
	}

	DDX_Text(pDX, IDC_PROPS_PATHNAME, fPathName);
}

/*
 * This is called when the file type selection changes or something is
 * typed in the aux type box.
 *
 * We use this notification to configure the type description field.
 *
 * Typing in the ProDOS aux type box causes us to nuke the HFS values.
 * If we were in "HFS mode" we reset the file type to zero.
 */
void
EditPropsDialog::OnTypeChange(void)
{
	static const char* kUnknownFileType = "Unknown file type";
	CComboBox* pCombo;
	CWnd* pWnd;
	int fileType, fileTypeIdx;
	long auxType;
	const char* descr = nil;

	pCombo = (CComboBox*) GetDlgItem(IDC_PROPS_FILETYPE);
	ASSERT(pCombo != nil);

	fileTypeIdx = pCombo->GetCurSel();
	fileType = pCombo->GetItemData(fileTypeIdx);
	if (fileType >= 256) {
		descr = kUnknownFileType;
	} else {
		auxType = GetAuxType();
		if (auxType < 0)
			auxType = 0;
		descr = PathProposal::FileTypeDescription(fileType, auxType);
		if (descr == nil)
			descr = kUnknownFileType;
	}

	pWnd = GetDlgItem(IDC_PROPS_TYPEDESCR);
	ASSERT(pWnd != nil);
	pWnd->SetWindowText(descr);

	/* DOS aux type only applies to BIN */
	if (!fReadOnly && fAllowedTypes == kAllowedDOS) {
		pWnd = GetDlgItem(IDC_PROPS_AUXTYPE);
		pWnd->EnableWindow(fileType == kFileTypeBIN);
	}
}

/*
 * Called when something is typed in one of the HFS type boxes.
 */
void
EditPropsDialog::OnHFSTypeChange(void)
{
	assert(fAllowedTypes == kAllowedHFS);
}

/*
 * Called initially and when switching modes.
 */
void
EditPropsDialog::UpdateHFSMode(void)
{
	CButton* pButton = (CButton*) GetDlgItem(IDC_PROPS_HFS_MODE);
	CComboBox* pCombo;
	CWnd* pWnd;

	if (pButton->GetCheck() == BST_CHECKED) {
		/* switch to HFS mode */
		WMSG0("Switching to HFS mode\n");
		//fHFSMode = true;

		pWnd = GetDlgItem(IDC_PROPS_HFS_FILETYPE);
		pWnd->EnableWindow(TRUE);
		pWnd = GetDlgItem(IDC_PROPS_HFS_AUXTYPE);
		pWnd->EnableWindow(TRUE);
		pWnd = GetDlgItem(IDC_PROPS_HFS_LABEL);
		pWnd->EnableWindow(TRUE);

		/* point the file type at something safe */
		pCombo = (CComboBox*) GetDlgItem(IDC_PROPS_FILETYPE);
		pCombo->EnableWindow(FALSE);

		pWnd = GetDlgItem(IDC_PROPS_AUXTYPE);
		pWnd->EnableWindow(FALSE);

		pWnd = GetDlgItem(IDC_PROPS_TYPEDESCR);
		ASSERT(pWnd != nil);
		pWnd->SetWindowText("(HFS type)");
		OnHFSTypeChange();
	} else {
		/* switch to ProDOS mode */
		WMSG0("Switching to ProDOS mode\n");
		//fHFSMode = false;
		pCombo = (CComboBox*) GetDlgItem(IDC_PROPS_FILETYPE);
		pCombo->EnableWindow(TRUE);
		pWnd = GetDlgItem(IDC_PROPS_AUXTYPE);
		pWnd->EnableWindow(TRUE);

		pWnd = GetDlgItem(IDC_PROPS_HFS_FILETYPE);
		pWnd->EnableWindow(FALSE);
		pWnd = GetDlgItem(IDC_PROPS_HFS_AUXTYPE);
		pWnd->EnableWindow(FALSE);
		pWnd = GetDlgItem(IDC_PROPS_HFS_LABEL);
		pWnd->EnableWindow(FALSE);
		OnTypeChange();
	}
}

/*
 * For "simple" access formats, i.e. DOS 3.2/3.3, the "write" button acts
 * as a "locked" flag.  We want the other rename/delete flags to track this
 * one.
 */
void
EditPropsDialog::UpdateSimpleAccess(void)
{
	if (!fSimpleAccess)
		return;

	CButton* pButton;
	UINT checked;

	pButton = (CButton*) GetDlgItem(IDC_PROPS_ACCESS_W);
	checked = pButton->GetCheck();

	pButton = (CButton*) GetDlgItem(IDC_PROPS_ACCESS_N);
	pButton->SetCheck(checked);
	pButton = (CButton*) GetDlgItem(IDC_PROPS_ACCESS_D);
	pButton->SetCheck(checked);
}


/*
 * Get the aux type.
 *
 * Returns -1 if something was wrong with the string (e.g. empty or has
 * invalid chars).
 */
long
EditPropsDialog::GetAuxType(void)
{
	CWnd* pWnd = GetDlgItem(IDC_PROPS_AUXTYPE);
	ASSERT(pWnd != nil);

	CString aux;
	pWnd->GetWindowText(aux);

	const char* str = aux;
	char* end;
	long val;

	if (str[0] == '\0') {
		WMSG0(" HEY: blank aux type, returning -1\n");
		return -1;
	}
	val = strtoul(aux, &end, 16);
	if (end != str + strlen(str)) {
		WMSG1(" HEY: found some garbage in aux type '%s', returning -1\n",
			(LPCTSTR) aux);
		return -1;
	}
	return val;
}

/*
 * Context help request (question mark button).
 */
BOOL
EditPropsDialog::OnHelpInfo(HELPINFO* lpHelpInfo)
{
	WinHelp((DWORD) lpHelpInfo->iCtrlId, HELP_CONTEXTPOPUP);
	return TRUE;	// yes, we handled it
}

/*
 * User pressed the "Help" button.
 */
void
EditPropsDialog::OnHelp(void)
{
	WinHelp(HELP_TOPIC_EDIT_PROPS, HELP_CONTEXT);
}
