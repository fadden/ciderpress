/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Disk editor implementation.
 *
 * Note to self: it should be possible to open an image in "nibble" mode,
 * switch to one of the various "sector" modes, and maybe even try out a
 * "block" mode for a while.  Locking the editor into one particular mode
 * makes for a cumbersome interface when dealing with certain kinds of disks.
 * It should also be possible to configure the "custom" NibbleDescr and then
 * re-analyze the disk, possibly transferring the customization over to
 * the main file listing.  (Perhaps the customization affects a global slot?
 * Probably want more than one set of custom nibble values, with the config
 * dialog accessible from main menu and from within disk editor.)
 *
 * A track copier would be neat too.
 */
#include "stdafx.h"
#include "SubVolumeDialog.h"
#include "DEFileDialog.h"
#include "HelpTopics.h"
#include "DiskEditDialog.h"


/*
 * ===========================================================================
 *		DiskEditDialog (base class)
 * ===========================================================================
 */

BEGIN_MESSAGE_MAP(DiskEditDialog, CDialog)
	ON_BN_CLICKED(IDC_DISKEDIT_DONE, OnDone)
	ON_BN_CLICKED(IDC_DISKEDIT_HEX, OnHexMode)
	ON_BN_CLICKED(IDC_DISKEDIT_DOREAD, OnDoRead)
	ON_BN_CLICKED(IDC_DISKEDIT_DOWRITE, OnDoWrite)
	ON_BN_CLICKED(IDC_DISKEDIT_PREV, OnReadPrev)
	ON_BN_CLICKED(IDC_DISKEDIT_NEXT, OnReadNext)
	ON_BN_CLICKED(IDC_DISKEDIT_SUBVOLUME, OnSubVolume)
	ON_BN_CLICKED(IDC_DISKEDIT_OPENFILE, OnOpenFile)
	ON_BN_CLICKED(IDHELP, OnHelp)
	ON_CBN_SELCHANGE(IDC_DISKEDIT_NIBBLE_PARMS, OnNibbleParms)
	ON_WM_HELPINFO()
END_MESSAGE_MAP()

/*
 * Initialize the controls.
 */
BOOL
DiskEditDialog::OnInitDialog(void)
{
	ASSERT(!fFileName.IsEmpty());
	ASSERT(fpDiskFS != nil);

	/*
	 * Disable the write button.
	 */
	if (fReadOnly) {
		CButton* pButton = (CButton*) GetDlgItem(IDC_DISKEDIT_DOWRITE);
		ASSERT(pButton != nil);
		pButton->EnableWindow(FALSE);
	}

	/*
	 * Use modified spin controls so we're not limited to 16 bits.
	 */
	ReplaceSpinCtrl(&fTrackSpinner, IDC_DISKEDIT_TRACKSPIN,
		IDC_DISKEDIT_TRACK);
	ReplaceSpinCtrl(&fSectorSpinner, IDC_DISKEDIT_SECTORSPIN,
		IDC_DISKEDIT_SECTOR);



	/*
	 * Configure the RichEdit control.
	 */
	CRichEditCtrl* pEdit = (CRichEditCtrl*) GetDlgItem(IDC_DISKEDIT_EDIT);
	ASSERT(pEdit != nil);

	/* set the font to 10-point Courier New */
	CHARFORMAT cf;
	cf.cbSize = sizeof(CHARFORMAT);
	cf.dwMask = CFM_FACE | CFM_SIZE;
	::lstrcpy(cf.szFaceName, "Courier New");
	cf.yHeight = 10 * 20;		// point size in twips
	BOOL cc = pEdit->SetDefaultCharFormat(cf);
	if (cc == FALSE) {
		WMSG0("SetDefaultCharFormat failed?\n");
		ASSERT(FALSE);
	}

	/* set to read-only */
	pEdit->SetReadOnly();
	/* retain the selection even if we lose focus [can't do this in OnInit] */
	pEdit->SetOptions(ECOOP_OR, ECO_SAVESEL);

	/*
	 * Disable the sub-volume and/or file open buttons if the DiskFS doesn't
	 * have the appropriate stuff inside.
	 */
	if (fpDiskFS->GetNextFile(nil) == nil) {
		CWnd* pWnd = GetDlgItem(IDC_DISKEDIT_OPENFILE);
		pWnd->EnableWindow(FALSE);
	}
	if (fpDiskFS->GetNextSubVolume(nil) == nil) {
		CWnd* pWnd = GetDlgItem(IDC_DISKEDIT_SUBVOLUME);
		pWnd->EnableWindow(FALSE);
	}

	/*
	 * Configure the nibble parm drop-list in an appropriate fashion.
	 */
	InitNibbleParmList();

	/*
	 * If this is a sub-volume edit window, pop us up slightly offset from the
	 * parent window so the user can see that there's more than one thing
	 * open.
	 */
	if (fPositionShift != 0) {
		CRect rect;
		GetWindowRect(&rect);
		rect.top += fPositionShift;
		rect.left += fPositionShift;
		rect.bottom += fPositionShift;
		rect.right += fPositionShift;
		MoveWindow(&rect);
	}

	/*
	 * Set the window title.
	 */
	CString title("Disk Viewer - ");
	title += fFileName;
	if (fpDiskFS->GetVolumeID() != nil) {
		title += " (";
		title += fpDiskFS->GetVolumeID();
		title += ")";
	}
	SetWindowText(title);

	return TRUE;
}

/*
 * Initialize the nibble parm drop-list.
 */
void
DiskEditDialog::InitNibbleParmList(void)
{
	ASSERT(fpDiskFS != nil);
	DiskImg* pDiskImg = fpDiskFS->GetDiskImg();
	CComboBox* pCombo;

	pCombo = (CComboBox*) GetDlgItem(IDC_DISKEDIT_NIBBLE_PARMS);
	ASSERT(pCombo != nil);

	if (pDiskImg->GetHasNibbles()) {
		const DiskImg::NibbleDescr* pTable;
		const DiskImg::NibbleDescr* pCurrent;
		int i, count;

		pTable = pDiskImg->GetNibbleDescrTable(&count);
		if (pTable == nil || count <= 0) {
			WMSG2("WHOOPS: nibble parm got table=0x%08lx, count=%d\n",
				(long) pTable, count);
			return;
		}
		pCurrent = pDiskImg->GetNibbleDescr();

		/* configure the selection to match the disk analysis */
		int dflt = -1;
		if (pCurrent != nil) {
			for (i = 0; i < count; i++) {
				if (memcmp(&pTable[i], pCurrent, sizeof(*pCurrent)) == 0) {
					WMSG1("  NibbleParm match on entry %d\n", i);
					dflt = i;
					break;
				}
			}

			if (dflt == -1) {
				WMSG0("  GLITCH: no match on nibble descr in table?!\n");
				dflt = 0;
			}
		}

		for (i = 0; i < count; i++) {
			if (pTable[i].numSectors > 0)
				pCombo->AddString(pTable[i].description);
			else {
				/* only expecting this on the last, "custom" entry */
				ASSERT(i == count-1);
			}
		}
		pCombo->SetCurSel(dflt);
	} else {
		pCombo->AddString("Nibble Parms");
		pCombo->SetCurSel(0);
		pCombo->EnableWindow(FALSE);
	}
}


/*
 * Replace a spin button with our improved version.
 */
int
DiskEditDialog::ReplaceSpinCtrl(MySpinCtrl* pNewSpin, int idSpin, int idEdit)
{
	CSpinButtonCtrl* pSpin;
//	CRect rect;
	DWORD style;
		
	pSpin = (CSpinButtonCtrl*)GetDlgItem(idSpin);
	if (pSpin == nil)
		return -1;
//	pSpin->GetWindowRect(&rect);
//	ScreenToClient(&rect);
	style = pSpin->GetStyle();
	style &= ~(UDS_SETBUDDYINT);
	//style |= UDS_AUTOBUDDY;
	ASSERT(!(style & UDS_AUTOBUDDY));
	pSpin->DestroyWindow();
	pNewSpin->Create(style, CRect(0,0,0,0), this, idSpin);
	pNewSpin->SetBuddy(GetDlgItem(idEdit));

	return 0;
}

/*
 * Special keypress handling.
 */
BOOL
DiskEditDialog::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN &&
         pMsg->wParam == VK_RETURN)
	{
		//WMSG0("RETURN!\n");
		LoadData();
		return TRUE;
	}

	return CDialog::PreTranslateMessage(pMsg);
}

/*
 * F1 key hit, or '?' button in title bar used to select help for an
 * item in the dialog.
 */
BOOL
DiskEditDialog::OnHelpInfo(HELPINFO* lpHelpInfo)
{
	WMSG4("HELP: size=%d contextType=%d ctrlID=0x%x contextID=0x%08lx\n",
		lpHelpInfo->cbSize, lpHelpInfo->iContextType, lpHelpInfo->iCtrlId,
		lpHelpInfo->dwContextId);

	DWORD context = lpHelpInfo->iCtrlId;

	/* map all of the track/sector selection stuff to one item */
	if (context == IDC_DISKEDIT_TRACK ||
		context == IDC_DISKEDIT_TRACKSPIN ||
		context == IDC_STEXT_TRACK ||
		context == IDC_DISKEDIT_SECTOR ||
		context == IDC_DISKEDIT_SECTORSPIN ||
		context == IDC_STEXT_SECTOR)
	{
		context = IDC_DISKEDIT_TRACK;
	}

	WinHelp(context, HELP_CONTEXTPOPUP);
	return TRUE;	// indicate success??
}

/*
 * User pressed the "Help" button.
 */
void
DiskEditDialog::OnHelp(void)
{
	WinHelp(HELP_TOPIC_DISKEDIT, HELP_CONTEXT);
}

/*
 * Handle the "Done" button.  We don't use IDOK because we don't want
 * <return> to bail out of the dialog.
 */
void
DiskEditDialog::OnDone(void)
{
	WMSG0("DiskEditDialog OnDone\n");
	EndDialog(IDOK);
}

/*
 * Toggle the spin button / edit controls.
 */
void
DiskEditDialog::OnHexMode(void)
{
	int base;

	CButton* pButton = (CButton*) GetDlgItem(IDC_DISKEDIT_HEX);
	ASSERT(pButton != nil);

	if (pButton->GetCheck() == 0)
		base = 10;
	else
		base = 16;

	SetSpinMode(IDC_DISKEDIT_TRACKSPIN, base);
	SetSpinMode(IDC_DISKEDIT_SECTORSPIN, base);
}

/*
 * Create a new instance of the disk edit dialog, for a sub-volume.
 */
void
DiskEditDialog::OnSubVolume(void)
{
	SubVolumeDialog subv(this);
	bool showAsBlocks;

	subv.Setup(fpDiskFS);
	if (subv.DoModal() == IDOK) {
		WMSG1("SELECTED subv %d\n", subv.fListBoxIndex);
		DiskFS::SubVolume* pSubVol = fpDiskFS->GetNextSubVolume(nil);
		if (pSubVol == nil)
			return;

		while (subv.fListBoxIndex-- > 0) {
			pSubVol = fpDiskFS->GetNextSubVolume(pSubVol);
		}
		ASSERT(pSubVol != nil);

		BlockEditDialog blockEdit;
		SectorEditDialog sectorEdit;
		DiskEditDialog* pEditDialog;

		showAsBlocks = pSubVol->GetDiskImg()->ShowAsBlocks();
		if (showAsBlocks)
			pEditDialog = &blockEdit;
		else
			pEditDialog = &sectorEdit;
		pEditDialog->Setup(pSubVol->GetDiskFS(), fpDiskFS->GetVolumeID());
		pEditDialog->SetPositionShift(8);
		(void) pEditDialog->DoModal();
	}
}

/*
 * Change the mode of a spin button.  The Windows control doesn't
 * immediately update with a hex display, so we do it manually.  (Our
 * replacement class does this correctly, but I'm leaving the code alone
 * for now.)
 */
void
DiskEditDialog::SetSpinMode(int id, int base)
{
	CString valStr;

	ASSERT(base == 10 || base == 16);

	MySpinCtrl* pSpin = (MySpinCtrl*) GetDlgItem(id);
	if (pSpin == nil) {
		// expected behavior in "block" mode for sector button
		WMSG1("Couldn't find spin button %d\n", id);
		return;
	}

	long val = pSpin->GetPos();
	if (val & 0xff000000) {
		WMSG0("NOTE: hex transition while value is invalid\n");
		val = 0;
	}

	if (base == 10)
		valStr.Format("%d", val);
	else
		valStr.Format("%X", val);

	pSpin->SetBase(base);
	pSpin->GetBuddy()->SetWindowText(valStr);
	WMSG2("Set spin button base to %d val=%d\n", base, val);
}

/*
 * Read a value from a spin control.
 *
 * Returns 0 on success, -1 if the return value from the spin control was
 * invalid.  In the latter case, an error dialog will be displayed.
 */
int
DiskEditDialog::ReadSpinner(int id, long* pVal)
{
	MySpinCtrl* pSpin = (MySpinCtrl*) GetDlgItem(id);
	ASSERT(pSpin != nil);

	long val = pSpin->GetPos();
	if (val & 0xff000000) {
		/* error */
		CString msg;
		CString err;
		err.LoadString(IDS_ERROR);
		int lower, upper;
		pSpin->GetRange32(lower, upper);
		msg.Format("Please enter a value between %d and %d (0x%x and 0x%x).",
			lower, upper, lower, upper);
		MessageBox(msg, err, MB_OK|MB_ICONEXCLAMATION);
		return -1;
	}

	*pVal = val;
	return 0;
}

/*
 * Set the value of a spin control.
 */
void
DiskEditDialog::SetSpinner(int id, long val)
{
	MySpinCtrl* pSpin = (MySpinCtrl*) GetDlgItem(id);
	ASSERT(pSpin != nil);

	/* sanity check */
	int lower, upper;
	pSpin->GetRange32(lower, upper);
	ASSERT(val >= lower && val <= upper);

	pSpin->SetPos(val);
}

/*
 * Convert a chunk of data into a hex dump, and stuff it into the edit control.
 */
void
DiskEditDialog::DisplayData(const unsigned char* srcBuf, int size)
{
	char textBuf[80 * 16 * 2];
	char* cp;
	int i, j;

	ASSERT(srcBuf != nil);
	ASSERT(size == kSectorSize || size == kBlockSize);

	CRichEditCtrl* pEdit = (CRichEditCtrl*)GetDlgItem(IDC_DISKEDIT_EDIT);
	ASSERT(pEdit != nil);

	/*
	 * If we have an alert message, show that instead.
	 */
	if (!fAlertMsg.IsEmpty()) {
		const int kWidth = 72;
		int indent = (kWidth/2) - (fAlertMsg.GetLength() / 2);
		if (indent < 0)
			indent = 0;

		CString msg = "                                    "
					  "                                    ";
		ASSERT(msg.GetLength() == kWidth);
		msg = msg.Left(indent);
		msg += fAlertMsg;
		for (i = 0; i < (size / 16)-2; i += 2) {
			textBuf[i] = '\r';
			textBuf[i+1] = '\n';
		}
		strcpy(&textBuf[i], msg);
		pEdit->SetWindowText(textBuf);

		return;
	}

	/*
	 * No alert, do the usual thing.
	 */
	cp = textBuf;
	for (i = 0; i < size/16; i++) {
		if (size == kSectorSize) {
			/* two-nybble addr */
			sprintf(cp, " %02x: %02x %02x %02x %02x %02x %02x %02x %02x "
						"%02x %02x %02x %02x %02x %02x %02x %02x  ",
				i * 16,
				srcBuf[0], srcBuf[1], srcBuf[2], srcBuf[3],
				srcBuf[4], srcBuf[5], srcBuf[6], srcBuf[7],
				srcBuf[8], srcBuf[9], srcBuf[10], srcBuf[11],
				srcBuf[12], srcBuf[13], srcBuf[14], srcBuf[15]);
		} else {
			/* three-nybble addr */
			sprintf(cp, "%03x: %02x %02x %02x %02x %02x %02x %02x %02x "
						"%02x %02x %02x %02x %02x %02x %02x %02x  ",
				i * 16,
				srcBuf[0], srcBuf[1], srcBuf[2], srcBuf[3],
				srcBuf[4], srcBuf[5], srcBuf[6], srcBuf[7],
				srcBuf[8], srcBuf[9], srcBuf[10], srcBuf[11],
				srcBuf[12], srcBuf[13], srcBuf[14], srcBuf[15]);
		}
		ASSERT(strlen(cp) == 54);
		cp += 54;		// strlen(cp)
		for (j = 0; j < 16; j++)
			*cp++ = PrintableChar(srcBuf[j]);

		*cp++ = '\r';
		*cp++ = '\n';

		srcBuf += 16;
	}
	/* kill the last EOL, so the cursor doesn't move past that line */
	cp--;
	*cp = '\0';

	pEdit->SetWindowText(textBuf);
}

/*
 * Display a track full of nibble data.
 */
void
DiskEditDialog::DisplayNibbleData(const unsigned char* srcBuf, int size)
{
	ASSERT(srcBuf != nil);
	ASSERT(size > 0);
	ASSERT(fAlertMsg.IsEmpty());

	int bufSize = ((size+15) / 16) * 80;
	char* textBuf = new char[bufSize];
	char* cp;
	int i;

	if (textBuf == nil)
		return;

	cp = textBuf;
	for (i = 0; size > 0; i++) {
		if (size >= 16) {
			sprintf(cp, "%04x: %02x %02x %02x %02x %02x %02x %02x %02x "
						"%02x %02x %02x %02x %02x %02x %02x %02x",
				i * 16,
				srcBuf[0], srcBuf[1], srcBuf[2], srcBuf[3],
				srcBuf[4], srcBuf[5], srcBuf[6], srcBuf[7],
				srcBuf[8], srcBuf[9], srcBuf[10], srcBuf[11],
				srcBuf[12], srcBuf[13], srcBuf[14], srcBuf[15]);
			ASSERT(strlen(cp) == 53);
			cp += 53;		// strlen(cp)
		} else {
			sprintf(cp, "%04x:", i * 16);
			cp += 5;
			for (int j = 0; j < size; j++) {
				sprintf(cp, " %02x", srcBuf[j]);
				cp += 3;
			}
		}

		*cp++ = '\r';
		*cp++ = '\n';

		srcBuf += 16;
		size -= 16;
	}
	/* kill the last EOL, so the cursor doesn't move past that line */
	cp--;
	*cp = '\0';

	CRichEditCtrl* pEdit = (CRichEditCtrl*)GetDlgItem(IDC_DISKEDIT_EDIT);
	ASSERT(pEdit != nil);
	pEdit->SetWindowText(textBuf);

	/*
	 * Handle resize of edit box.  We have to do this late or the scroll bar
	 * won't appear under Win98.  (Whatever.)
	 */
	if (fFirstResize) {
		fFirstResize = false;

		const int kStretchHeight = 249;
		CRect rect;

		GetWindowRect(&rect);

		CRect inner;
		pEdit->GetRect(&inner);
		inner.bottom += kStretchHeight;
		pEdit->GetWindowRect(&rect);
		ScreenToClient(&rect);
		rect.bottom += kStretchHeight;
		pEdit->MoveWindow(&rect);
		pEdit->SetRect(&inner);
	}

	delete[] textBuf;
}


/*
 * Open a file in a disk image.
 *
 * Returns a pointer to the A2File and A2FileDescr structures on success, nil
 * on failure.  The pointer placed in "ppOpenFile" must be freed by invoking
 * its Close function.
 */
DIError
DiskEditDialog::OpenFile(const char* fileName, bool openRsrc, A2File** ppFile,
	A2FileDescr** ppOpenFile)
{
	A2File* pFile;
	A2FileDescr* pOpenFile = nil;

	WMSG2(" OpenFile '%s' rsrc=%d\n", fileName, openRsrc);
	pFile = fpDiskFS->GetFileByName(fileName);
	if (pFile == nil) {
		CString msg, failed;

		msg.Format(IDS_DEFILE_FIND_FAILED, fileName);
		failed.LoadString(IDS_FAILED);
		MessageBox(msg, failed, MB_OK | MB_ICONSTOP);
		return kDIErrFileNotFound;
	}

	DIError dierr;
	dierr = pFile->Open(&pOpenFile, true, openRsrc);
	if (dierr != kDIErrNone) {
		CString msg, failed;

		msg.Format(IDS_DEFILE_OPEN_FAILED, fileName,
			DiskImgLib::DIStrError(dierr));
		failed.LoadString(IDS_FAILED);
		MessageBox(msg, failed, MB_OK | MB_ICONSTOP);
		return dierr;
	}

	*ppFile = pFile;
	*ppOpenFile = pOpenFile;

	return kDIErrNone;
}


/*
 * Change the nibble parms.
 *
 * Assumes the parm list is linear and unbroken.
 */
void
DiskEditDialog::OnNibbleParms(void)
{
	DiskImg* pDiskImg = fpDiskFS->GetDiskImg();
	CComboBox* pCombo;
	int sel;

	ASSERT(pDiskImg != nil);
	ASSERT(pDiskImg->GetHasNibbles());

	pCombo = (CComboBox*) GetDlgItem(IDC_DISKEDIT_NIBBLE_PARMS);
	ASSERT(pCombo != nil);

	sel = pCombo->GetCurSel();
	if (sel == CB_ERR)
		return;

	WMSG1(" OnNibbleParms: entry %d now selected\n", sel);
	const DiskImg::NibbleDescr* pNibbleTable;
	int count;
	pNibbleTable = pDiskImg->GetNibbleDescrTable(&count);
	ASSERT(sel < count);
	pDiskImg->SetNibbleDescr(sel);

	LoadData();
}


#if 0
/*
 * Make a "sparse" block in a file obvious by filling it with the word
 * "sparse".
 */
void
DiskEditDialog::FillWithPattern(unsigned char* buf, int size,
	const char* pattern)
{
	const char* cp;
	unsigned char* ucp;

	ucp = buf;
	cp = pattern;
	while (ucp < buf+size) {
		*ucp++ = *cp++;
		if (*cp == '\0')
			cp = pattern;
	}
}
#endif


/*
 * ===========================================================================
 *		SectorEditDialog
 * ===========================================================================
 */

/*
 * Prep the dialog.
 */
BOOL
SectorEditDialog::OnInitDialog(void)
{
	/*
	 * Do base-class construction.
	 */
	DiskEditDialog::OnInitDialog();

	/*
	 * Change track/sector text.
	 */
	CString trackStr;
	CWnd* pWnd;
	trackStr.Format("Track (%d):", fpDiskFS->GetDiskImg()->GetNumTracks());
	pWnd = GetDlgItem(IDC_STEXT_TRACK);
	ASSERT(pWnd != nil);
	pWnd->SetWindowText(trackStr);
	trackStr.Format("Sector (%d):", fpDiskFS->GetDiskImg()->GetNumSectPerTrack());
	pWnd = GetDlgItem(IDC_STEXT_SECTOR);
	ASSERT(pWnd != nil);
	pWnd->SetWindowText(trackStr);

	/*
	 * Configure the spin buttons.
	 */
	MySpinCtrl* pSpin;
	pSpin = (MySpinCtrl*)GetDlgItem(IDC_DISKEDIT_TRACKSPIN);
	ASSERT(pSpin != nil);
	pSpin->SetRange32(0, fpDiskFS->GetDiskImg()->GetNumTracks()-1);
	pSpin->SetPos(0);
	pSpin = (MySpinCtrl*)GetDlgItem(IDC_DISKEDIT_SECTORSPIN);
	ASSERT(pSpin != nil);
	pSpin->SetRange32(0, fpDiskFS->GetDiskImg()->GetNumSectPerTrack()-1);
	pSpin->SetPos(0);

	/* give us something to look at */
	LoadData();

	return TRUE;
}

/*
 * Load the current track/sector data into the edit control.
 *
 * Returns 0 on success, -1 on error.
 */
int
SectorEditDialog::LoadData(void)
{
	//WMSG0("SED LoadData\n");
	ASSERT(fpDiskFS != nil);
	ASSERT(fpDiskFS->GetDiskImg() != nil);

	if (ReadSpinner(IDC_DISKEDIT_TRACKSPIN, &fTrack) != 0)
		return -1;
	if (ReadSpinner(IDC_DISKEDIT_SECTORSPIN, &fSector) != 0)
		return -1;

	WMSG2("LoadData reading t=%d s=%d\n", fTrack, fSector);

	fAlertMsg = "";
	DIError dierr;
	dierr = fpDiskFS->GetDiskImg()->ReadTrackSector(fTrack, fSector, fSectorData);
	if (dierr != kDIErrNone) {
		WMSG1("SED sector read failed: %s\n", DiskImgLib::DIStrError(dierr));
		//CString msg;
		//CString err;
		//err.LoadString(IDS_ERROR);
		//msg.Format(IDS_DISKEDIT_NOREADTS, fTrack, fSector);
		//MessageBox(msg, err, MB_OK|MB_ICONSTOP);
		fAlertMsg.LoadString(IDS_DISKEDITMSG_BADSECTOR);
		//return -1;
	}

	DisplayData();

	return 0;
}

/*
 * Read the currently specified track/sector.
 */
void
SectorEditDialog::OnDoRead(void)
{
	LoadData();
}

/*
 * Write the currently loaded track/sector.
 */
void
SectorEditDialog::OnDoWrite(void)
{
	MessageBox("Write!");
}

/*
 * Back up to the previous track/sector.
 */
void
SectorEditDialog::OnReadPrev(void)
{
	if (fTrack == 0 && fSector == 0)
		return;

	if (fSector == 0) {
		fSector = fpDiskFS->GetDiskImg()->GetNumSectPerTrack() -1;
		fTrack--;
	} else {
		fSector--;
	}

	SetSpinner(IDC_DISKEDIT_TRACKSPIN, fTrack);
	SetSpinner(IDC_DISKEDIT_SECTORSPIN, fSector);

	LoadData();
}

/*
 * Same as OnReadPrev, but moving forward.
 */
void
SectorEditDialog::OnReadNext(void)
{
	int numTracks = fpDiskFS->GetDiskImg()->GetNumTracks();
	int numSects = fpDiskFS->GetDiskImg()->GetNumSectPerTrack();

	if (fTrack == numTracks-1 && fSector == numSects-1)
		return;

	if (fSector == numSects-1) {
		fSector = 0;
		fTrack++;
	} else {
		fSector++;
	}

	SetSpinner(IDC_DISKEDIT_TRACKSPIN, fTrack);
	SetSpinner(IDC_DISKEDIT_SECTORSPIN, fSector);

	LoadData();
}


/*
 * Open a file on the disk image.  If successful, open a new edit dialog
 * that's in "file follow" mode.
 */
void
SectorEditDialog::OnOpenFile(void)
{
	DEFileDialog fileDialog(this);

	if (fileDialog.DoModal() == IDOK) {
		SectorFileEditDialog fileEdit(this, this);
		A2File* pFile;
		A2FileDescr* pOpenFile = nil;
		DIError dierr;

		dierr = OpenFile(fileDialog.fName, fileDialog.fOpenRsrcFork != 0,
					&pFile, &pOpenFile);
		if (dierr != kDIErrNone)
			return;

		fileEdit.SetupFile(fileDialog.fName, fileDialog.fOpenRsrcFork != 0,
			pFile, pOpenFile);
		fileEdit.SetPositionShift(8);
		(void) fileEdit.DoModal();

		pOpenFile->Close();
	}
}


/*
 * ===========================================================================
 *		SectorFileEditDialog
 * ===========================================================================
 */

/*
 * Minor changes for file editing.
 */
BOOL
SectorFileEditDialog::OnInitDialog(void)
{
	BOOL retval;

	/* do base class first */
	retval = SectorEditDialog::OnInitDialog();

	/* disable direct entry of tracks and sectors */
	CWnd* pWnd;
	pWnd = GetDlgItem(IDC_DISKEDIT_TRACKSPIN);
	ASSERT(pWnd != nil);
	pWnd->EnableWindow(FALSE);
	pWnd = GetDlgItem(IDC_DISKEDIT_SECTORSPIN);
	ASSERT(pWnd != nil);
	pWnd->EnableWindow(FALSE);

	/* disallow opening of sub-volumes and files */
	pWnd = GetDlgItem(IDC_DISKEDIT_OPENFILE);
	pWnd->EnableWindow(FALSE);
	pWnd = GetDlgItem(IDC_DISKEDIT_SUBVOLUME);
	pWnd->EnableWindow(FALSE);

	CEdit* pEdit;
	pEdit = (CEdit*) GetDlgItem(IDC_DISKEDIT_TRACK);
	ASSERT(pEdit != nil);
	pEdit->SetReadOnly(TRUE);
	pEdit = (CEdit*) GetDlgItem(IDC_DISKEDIT_SECTOR);
	ASSERT(pEdit != nil);
	pEdit->SetReadOnly(TRUE);

	/* set the window title */
	CString title;
	CString rsrcIndic;
	rsrcIndic.LoadString(IDS_INDIC_RSRC);
	title.Format("Disk Viewer - %s%s (%ld bytes)",
		fpFile->GetPathName(),	// use fpFile version to get case
		fOpenRsrcFork ? (LPCTSTR)rsrcIndic : "", fLength);
	SetWindowText(title);

	return retval;
}

/*
 * Load data from the current offset into the edit control.
 *
 * Returns 0 on success, -1 on error.
 */
int
SectorFileEditDialog::LoadData(void)
{
	ASSERT(fpDiskFS != nil);
	ASSERT(fpDiskFS->GetDiskImg() != nil);

	DIError dierr;
	WMSG1("SFED LoadData reading index=%d\n", fSectorIdx);

#if 0
	WMSG1("LoadData reading offset=%d\n", fOffset);
	size_t actual = 0;
	dierr = fpFile->Seek(fOffset, EmbeddedFD::kSeekSet);
	if (dierr == kDIErrNone) {
		dierr = fpFile->Read(fSectorData, 1 /*kSectorSize*/, &actual);
	}
	if (dierr != kDIErrNone) {
		CString msg, failed;
		failed.LoadString(IDS_FAILED);
		msg.Format(IDS_DISKEDIT_FIRDFAILED, DiskImg::DIStrError(dierr));
		MessageBox(msg, failed, MB_OK);
		// TO DO: mark contents as invalid, so editing fails
		return -1;
	}

	if (actual != kSectorSize) {
		WMSG1(" SFED partial read of %d bytes\n", actual);
		ASSERT(actual < kSectorSize && actual >= 0);
	}

	/*
	 * We've read the data, but we can't use it.  We're a sector editor,
	 * not a file editor, and we need to get the actual sector data without
	 * EOF trimming or CP/M 0xe5 removal.
	 */
	fpFile->GetLastLocationRead(&fTrack, &fSector);
	if (fTrack == A2File::kLastWasSparse && fSector == A2File::kLastWasSparse)

		;
#endif

	fAlertMsg = "";

	dierr = fpOpenFile->GetStorage(fSectorIdx, &fTrack, &fSector);
	if (dierr == kDIErrInvalidIndex && fSectorIdx == 0) {
		// no first sector; should only happen on CP/M
		//FillWithPattern(fSectorData, sizeof(fSectorData), _T("EMPTY "));
		fAlertMsg.LoadString(IDS_DISKEDITMSG_EMPTY);
	} else if (dierr != kDIErrNone) {
		CString msg, failed;
		failed.LoadString(IDS_FAILED);
		msg.Format(IDS_DISKEDIT_FIRDFAILED, DiskImgLib::DIStrError(dierr));
		MessageBox(msg, failed, MB_OK);
		fAlertMsg.LoadString(IDS_FAILED);
		// TO DO: mark contents as invalid, so editing fails
		return -1;
	} else {
		if (fTrack == 0 && fSector == 0) {
			WMSG0("LoadData Sparse sector\n");
			//FillWithPattern(fSectorData, sizeof(fSectorData), _T("SPARSE "));
			fAlertMsg.Format(IDS_DISKEDITMSG_SPARSE, fSectorIdx);
		} else {
			WMSG2("LoadData reading T=%d S=%d\n", fTrack, fSector);

			dierr = fpDiskFS->GetDiskImg()->ReadTrackSector(fTrack, fSector,
						fSectorData);
			if (dierr != kDIErrNone) {
				//CString msg;
				//CString err;
				//err.LoadString(IDS_ERROR);
				//msg.Format(IDS_DISKEDIT_NOREADTS, fTrack, fSector);
				//MessageBox(msg, err, MB_OK|MB_ICONSTOP);
				fAlertMsg.LoadString(IDS_DISKEDITMSG_BADSECTOR);
				//return -1;
			}
		}
	}

	SetSpinner(IDC_DISKEDIT_TRACKSPIN, fTrack);
	SetSpinner(IDC_DISKEDIT_SECTORSPIN, fSector);

	CWnd* pWnd;
	pWnd = GetDlgItem(IDC_DISKEDIT_PREV);
	ASSERT(pWnd != nil);
	pWnd->EnableWindow(fSectorIdx > 0);
	if (!pWnd->IsWindowEnabled() && GetFocus() == nil)
		GetDlgItem(IDC_DISKEDIT_NEXT)->SetFocus();

	pWnd = GetDlgItem(IDC_DISKEDIT_NEXT);
	ASSERT(pWnd != nil);
	pWnd->EnableWindow(fSectorIdx+1 < fpOpenFile->GetSectorCount());
	if (!pWnd->IsWindowEnabled() && GetFocus() == nil)
		GetDlgItem(IDC_DISKEDIT_PREV)->SetFocus();

	DisplayData();

	return 0;
}

/*
 * Move to the previous sector in the file.
 */
void
SectorFileEditDialog::OnReadPrev(void)
{
	if (fSectorIdx == 0)
		return;

	fSectorIdx--;
	ASSERT(fSectorIdx >= 0);
	LoadData();
}

/*
 * Move to the next sector in the file.
 */
void
SectorFileEditDialog::OnReadNext(void)
{
	if (fSectorIdx+1 >= fpOpenFile->GetSectorCount())
		return;

	fSectorIdx++;
	ASSERT(fSectorIdx < fpOpenFile->GetSectorCount());
	LoadData();
}


/*
 * ===========================================================================
 *		BlockEditDialog
 * ===========================================================================
 */

/*
 * Rearrange the DiskEdit dialog (which defaults to SectorEdit mode) to
 * accommodate block editing.
 */
BOOL
BlockEditDialog::OnInitDialog(void)
{
	/*
	 * Get rid of the "sector" input item, and change the "track" input
	 * item to accept blocks instead.
	 */
	CWnd* pWnd;

	pWnd = GetDlgItem(IDC_STEXT_SECTOR);
	pWnd->DestroyWindow();
	pWnd = GetDlgItem(IDC_DISKEDIT_SECTOR);
	pWnd->DestroyWindow();
	pWnd = GetDlgItem(IDC_DISKEDIT_SECTORSPIN);
	pWnd->DestroyWindow();

	CString blockStr;
	//blockStr.LoadString(IDS_BLOCK);
	blockStr.Format("Block (%d):", fpDiskFS->GetDiskImg()->GetNumBlocks());
	pWnd = GetDlgItem(IDC_STEXT_TRACK);
	ASSERT(pWnd != nil);
	pWnd->SetWindowText(blockStr);

	/*
	 * Increase the size of the window to accommodate the larger block size.
	 */
	const int kStretchHeight = 250;
	CRect rect;

	GetWindowRect(&rect);
	rect.bottom += kStretchHeight;
	MoveWindow(&rect);

	CRichEditCtrl* pEdit = (CRichEditCtrl*)GetDlgItem(IDC_DISKEDIT_EDIT);
	ASSERT(pEdit != nil);
	CRect inner;
	pEdit->GetRect(&inner);
	inner.bottom += kStretchHeight;
	pEdit->GetWindowRect(&rect);
	ScreenToClient(&rect);
	rect.bottom += kStretchHeight;
	pEdit->MoveWindow(&rect);
	pEdit->SetRect(&inner);

	MoveControl(this, IDC_DISKEDIT_DONE, 0, kStretchHeight);
	MoveControl(this, IDC_DISKEDIT_OPENFILE, 0, kStretchHeight);
	MoveControl(this, IDC_DISKEDIT_SUBVOLUME, 0, kStretchHeight);
	MoveControl(this, IDHELP, 0, kStretchHeight);
	MoveControl(this, IDC_DISKEDIT_NIBBLE_PARMS, 0, kStretchHeight);

	/*
	 * Do base-class construction.
	 */
	DiskEditDialog::OnInitDialog();

	/*
	 * Configure the spin button.  We use the "track" spin button for blocks.
	 */
	MySpinCtrl* pSpin;
	pSpin = (MySpinCtrl*)GetDlgItem(IDC_DISKEDIT_TRACKSPIN);
	ASSERT(pSpin != nil);
	pSpin->SetRange32(0, fpDiskFS->GetDiskImg()->GetNumBlocks()-1);
	pSpin->SetPos(0);

	/* give us something to look at */
	if (LoadData() != 0) {
		WMSG0("WHOOPS: LoadData() failed, but we're in OnInitDialog\n");
	}

	return TRUE;
}

#if 0
/*
 * Move a control so it maintains its same position relative to the bottom
 * and right edges.
 */
void
BlockEditDialog::MoveControl(int id, int deltaX, int deltaY)
{
	CWnd* pWnd;
	CRect rect;

	pWnd = GetDlgItem(id);
	ASSERT(pWnd != nil);

	pWnd->GetWindowRect(&rect);
	ScreenToClient(&rect);
	rect.left += deltaX;
	rect.right += deltaX;
	rect.top += deltaY;
	rect.bottom += deltaY;
	pWnd->MoveWindow(&rect, TRUE);
}
#endif


/*
 * Load the current block data into the edit control.
 */
int
BlockEditDialog::LoadData(void)
{
	//WMSG0("BED LoadData\n");
	ASSERT(fpDiskFS != nil);
	ASSERT(fpDiskFS->GetDiskImg() != nil);

	if (ReadSpinner(IDC_DISKEDIT_TRACKSPIN, &fBlock) != 0)
		return -1;

	WMSG1("LoadData reading block=%d\n", fBlock);

	fAlertMsg = "";
	DIError dierr;
	dierr = fpDiskFS->GetDiskImg()->ReadBlock(fBlock, fBlockData);
	if (dierr != kDIErrNone) {
		WMSG1("BED block read failed: %s\n", DiskImgLib::DIStrError(dierr));
		//CString msg;
		//CString err;
		//err.LoadString(IDS_ERROR);
		//msg.Format(IDS_DISKEDIT_NOREADBLOCK, fBlock);
		//MessageBox(msg, err, MB_OK|MB_ICONSTOP);
		fAlertMsg.LoadString(IDS_DISKEDITMSG_BADBLOCK);
		//return -1;
	}

	DisplayData();

	return 0;
}

/*
 * Read the currently specified track/sector.
 */
void
BlockEditDialog::OnDoRead(void)
{
	LoadData();
}

/*
 * Write the currently loaded track/sector.
 */
void
BlockEditDialog::OnDoWrite(void)
{
	MessageBox("Write!");
}

/*
 * Back up to the previous track/sector, or (in follow-file mode) to the
 * previous sector in the file.
 */
void
BlockEditDialog::OnReadPrev(void)
{
	if (fBlock == 0)
		return;

	fBlock--;
	SetSpinner(IDC_DISKEDIT_TRACKSPIN, fBlock);
	LoadData();
}

/*
 * Same as OnReadPrev, but moving forward.
 */
void
BlockEditDialog::OnReadNext(void)
{
	ASSERT(fpDiskFS != nil);
	if (fBlock == fpDiskFS->GetDiskImg()->GetNumBlocks() - 1)
		return;

	fBlock++;
	SetSpinner(IDC_DISKEDIT_TRACKSPIN, fBlock);
	LoadData();
}

/*
 * Open a file on the disk image.  If successful, open a new edit dialog
 * that's in "file follow" mode.
 */
void
BlockEditDialog::OnOpenFile(void)
{
	DEFileDialog fileDialog(this);

	if (fileDialog.DoModal() == IDOK) {
		BlockFileEditDialog fileEdit(this, this);
		A2File* pFile;
		A2FileDescr* pOpenFile = nil;
		DIError dierr;

		dierr = OpenFile(fileDialog.fName, fileDialog.fOpenRsrcFork != 0,
					&pFile, &pOpenFile);
		if (dierr != kDIErrNone)
			return;

		fileEdit.SetupFile(fileDialog.fName, fileDialog.fOpenRsrcFork != 0,
			pFile, pOpenFile);
		fileEdit.SetPositionShift(8);
		(void) fileEdit.DoModal();

		pOpenFile->Close();
	}
}


/*
 * ===========================================================================
 *		BlockFileEditDialog
 * ===========================================================================
 */

/*
 * Minor changes for file editing.
 */
BOOL
BlockFileEditDialog::OnInitDialog(void)
{
	BOOL retval;

	/* do base class first */
	retval = BlockEditDialog::OnInitDialog();

	/* disable direct entry of tracks and Blocks */
	CWnd* pWnd;
	pWnd = GetDlgItem(IDC_DISKEDIT_TRACKSPIN);
	ASSERT(pWnd != nil);
	pWnd->EnableWindow(FALSE);

	/* disallow opening of sub-volumes and files */
	pWnd = GetDlgItem(IDC_DISKEDIT_OPENFILE);
	pWnd->EnableWindow(FALSE);
	pWnd = GetDlgItem(IDC_DISKEDIT_SUBVOLUME);
	pWnd->EnableWindow(FALSE);

	CEdit* pEdit;
	pEdit = (CEdit*) GetDlgItem(IDC_DISKEDIT_TRACK);
	ASSERT(pEdit != nil);
	pEdit->SetReadOnly(TRUE);

	/* set the window title */
	CString title;
	CString rsrcIndic;
	rsrcIndic.LoadString(IDS_INDIC_RSRC);
	title.Format("Disk Viewer - %s%s (%ld bytes)",
		fpFile->GetPathName(),	// use fpFile version to get case
		fOpenRsrcFork ? (LPCTSTR)rsrcIndic : "", fLength);
	SetWindowText(title);

	return retval;
}

/*
 * Load data from the current offset into the edit control.
 *
 * Returns 0 on success, -1 on error.
 */
int
BlockFileEditDialog::LoadData(void)
{
	ASSERT(fpDiskFS != nil);
	ASSERT(fpDiskFS->GetDiskImg() != nil);

	DIError dierr;
	WMSG1("BFED LoadData reading index=%d\n", fBlockIdx);

#if 0
	WMSG1("LoadData reading offset=%d\n", fOffset);
	size_t actual = 0;
	dierr = fpFile->Seek(fOffset, EmbeddedFD::kSeekSet);
	if (dierr == kDIErrNone) {
		dierr = fpFile->Read(fBlockData, 1 /*kBlockSize*/, &actual);
	}
	if (dierr != kDIErrNone) {
		CString msg, failed;
		failed.LoadString(IDS_FAILED);
		msg.Format(IDS_DISKEDIT_FIRDFAILED, DiskImg::DIStrError(dierr));
		MessageBox(msg, failed, MB_OK);
		// TO DO: mark contents as invalid, so editing fails
		return -1;
	}

	if (actual != kBlockSize) {
		WMSG1(" BFED partial read of %d bytes\n", actual);
		ASSERT(actual < kBlockSize && actual >= 0);
	}

	/*
	 * We've read the data, but we can't use it.  We're a Block editor,
	 * not a file editor, and we need to get the actual Block data without
	 * EOF trimming or CP/M 0xe5 removal.
	 */
	fpFile->GetLastLocationRead(&fBlock);
	if (fBlock == A2File::kLastWasSparse)
		;
#endif

	fAlertMsg = "";

	dierr = fpOpenFile->GetStorage(fBlockIdx, &fBlock);
	if (dierr == kDIErrInvalidIndex && fBlockIdx == 0) {
		// no first sector; should only happen on CP/M
		//FillWithPattern(fBlockData, sizeof(fBlockData), _T("EMPTY "));
		fAlertMsg.LoadString(IDS_DISKEDITMSG_EMPTY);
	} else if (dierr != kDIErrNone) {
		CString msg, failed;
		failed.LoadString(IDS_FAILED);
		msg.Format(IDS_DISKEDIT_FIRDFAILED, DiskImgLib::DIStrError(dierr));
		MessageBox(msg, failed, MB_OK);
		fAlertMsg.LoadString(IDS_FAILED);
		// TO DO: mark contents as invalid, so editing fails
		return -1;
	} else {
		if (fBlock == 0) {
			WMSG0("LoadData Sparse block\n");
			//FillWithPattern(fBlockData, sizeof(fBlockData), _T("SPARSE "));
			fAlertMsg.Format(IDS_DISKEDITMSG_SPARSE, fBlockIdx);
		} else {
			WMSG1("LoadData reading B=%d\n", fBlock);

			dierr = fpDiskFS->GetDiskImg()->ReadBlock(fBlock, fBlockData);
			if (dierr != kDIErrNone) {
				//CString msg;
				//CString err;
				//err.LoadString(IDS_ERROR);
				//msg.Format(IDS_DISKEDIT_NOREADBLOCK, fBlock);
				//MessageBox(msg, err, MB_OK|MB_ICONSTOP);
				fAlertMsg.LoadString(IDS_DISKEDITMSG_BADBLOCK);
				//return -1;
			}
		}
	}

	SetSpinner(IDC_DISKEDIT_TRACKSPIN, fBlock);

	CWnd* pWnd;
	pWnd = GetDlgItem(IDC_DISKEDIT_PREV);
	ASSERT(pWnd != nil);
	pWnd->EnableWindow(fBlockIdx > 0);
	if (!pWnd->IsWindowEnabled() && GetFocus() == nil)
		GetDlgItem(IDC_DISKEDIT_NEXT)->SetFocus();

	pWnd = GetDlgItem(IDC_DISKEDIT_NEXT);
	ASSERT(pWnd != nil);
	pWnd->EnableWindow(fBlockIdx+1 < fpOpenFile->GetBlockCount());
	if (!pWnd->IsWindowEnabled() && GetFocus() == nil)
		GetDlgItem(IDC_DISKEDIT_PREV)->SetFocus();

	DisplayData();

	return 0;
}

/*
 * Move to the previous Block in the file.
 */
void
BlockFileEditDialog::OnReadPrev(void)
{
	if (fBlockIdx == 0)
		return;

	fBlockIdx--;
	ASSERT(fBlockIdx >= 0);
	LoadData();
}

/*
 * Move to the next Block in the file.
 */
void
BlockFileEditDialog::OnReadNext(void)
{
	if (fBlockIdx+1 >= fpOpenFile->GetBlockCount())
		return;

	fBlockIdx++;
	ASSERT(fBlockIdx < fpOpenFile->GetBlockCount());
	LoadData();
}


/*
 * ===========================================================================
 *		NibbleEditDialog
 * ===========================================================================
 */

/*
 * Rearrange the DiskEdit dialog (which defaults to SectorEdit mode) to
 * accommodate nibble editing.
 */
BOOL
NibbleEditDialog::OnInitDialog(void)
{
	/*
	 * Get rid of the "sector" input item.
	 */
	CWnd* pWnd;

	pWnd = GetDlgItem(IDC_STEXT_SECTOR);
	pWnd->DestroyWindow();
	pWnd = GetDlgItem(IDC_DISKEDIT_SECTOR);
	pWnd->DestroyWindow();
	pWnd = GetDlgItem(IDC_DISKEDIT_SECTORSPIN);
	pWnd->DestroyWindow();

	CString trackStr;
	trackStr.Format("Track (%d):", fpDiskFS->GetDiskImg()->GetNumTracks());
	pWnd = GetDlgItem(IDC_STEXT_TRACK);
	ASSERT(pWnd != nil);
	pWnd->SetWindowText(trackStr);

	/*
	 * Increase the size of the window so it's the same height as blocks.
	 *
	 * NOTE: using a pixel constant is probably bad.  We want to use something
	 * like GetTextMetrics, but I'm not sure how to get that without a
	 * device context.
	 */
	CRichEditCtrl* pEdit = (CRichEditCtrl*)GetDlgItem(IDC_DISKEDIT_EDIT);
	ASSERT(pEdit != nil);
	const int kStretchHeight = 249;
	CRect rect;

	GetWindowRect(&rect);
	rect.bottom += kStretchHeight;
	MoveWindow(&rect);

	/*
	 * Must postpone resize of edit ctrl until after data has been loaded, or
	 * scroll bars fail to appear under Win98.  Makes no sense whatsoever, but
	 * that's Windows for you.
	 */
#if 0
	CRect inner;
	pEdit->GetRect(&inner);
	inner.bottom += kStretchHeight;
	pEdit->GetWindowRect(&rect);
	ScreenToClient(&rect);
	rect.bottom += kStretchHeight;
	pEdit->MoveWindow(&rect);
	pEdit->SetRect(&inner);
#endif

	/* show the scroll bar */
	pEdit->ShowScrollBar(SB_VERT);

	MoveControl(this, IDC_DISKEDIT_DONE, 0, kStretchHeight);
	MoveControl(this, IDC_DISKEDIT_OPENFILE, 0, kStretchHeight);
	MoveControl(this, IDC_DISKEDIT_SUBVOLUME, 0, kStretchHeight);
	MoveControl(this, IDHELP, 0, kStretchHeight);
	MoveControl(this, IDC_DISKEDIT_NIBBLE_PARMS, 0, kStretchHeight);

	/* disable opening of files and sub-volumes */
	pWnd = GetDlgItem(IDC_DISKEDIT_OPENFILE);
	pWnd->EnableWindow(FALSE);
	pWnd = GetDlgItem(IDC_DISKEDIT_SUBVOLUME);
	pWnd->EnableWindow(FALSE);

	/*
	 * Do base-class construction.
	 */
	DiskEditDialog::OnInitDialog();

	/*
	 * This currently has no effect on the nibble editor.  Someday we may
	 * want to highlight and/or decode address fields.
	 */
	pWnd = GetDlgItem(IDC_DISKEDIT_NIBBLE_PARMS);
	pWnd->EnableWindow(FALSE);

	/*
	 * Configure the track spin button.
	 */
	MySpinCtrl* pSpin;
	pSpin = (MySpinCtrl*)GetDlgItem(IDC_DISKEDIT_TRACKSPIN);
	ASSERT(pSpin != nil);
	pSpin->SetRange32(0, fpDiskFS->GetDiskImg()->GetNumTracks()-1);
	pSpin->SetPos(0);

	/* give us something to look at */
	LoadData();

	return TRUE;
}

/*
 * Load the current track data into the edit control.
 */
int
NibbleEditDialog::LoadData(void)
{
	//WMSG0("BED LoadData\n");
	ASSERT(fpDiskFS != nil);
	ASSERT(fpDiskFS->GetDiskImg() != nil);

	if (ReadSpinner(IDC_DISKEDIT_TRACKSPIN, &fTrack) != 0)
		return -1;

	WMSG1("LoadData reading track=%d\n", fTrack);

	fAlertMsg = "";
	DIError dierr;
	dierr = fpDiskFS->GetDiskImg()->ReadNibbleTrack(fTrack, fNibbleData,
				&fNibbleDataLen);
	if (dierr != kDIErrNone) {
		WMSG1("NED track read failed: %s\n", DiskImgLib::DIStrError(dierr));
		fAlertMsg.LoadString(IDS_DISKEDITMSG_BADTRACK);
	}

	DisplayData();

	return 0;
}

/*
 * Read the currently specified track/sector.
 */
void
NibbleEditDialog::OnDoRead(void)
{
	LoadData();
}

/*
 * Write the currently loaded track/sector.
 */
void
NibbleEditDialog::OnDoWrite(void)
{
	MessageBox("Write!");
}

/*
 * Back up to the previous track/sector, or (in follow-file mode) to the
 * previous sector in the file.
 */
void
NibbleEditDialog::OnReadPrev(void)
{
	if (fTrack == 0)
		return;

	fTrack--;
	SetSpinner(IDC_DISKEDIT_TRACKSPIN, fTrack);
	LoadData();
}

/*
 * Same as OnReadPrev, but moving forward.
 */
void
NibbleEditDialog::OnReadNext(void)
{
	ASSERT(fpDiskFS != nil);
	if (fTrack == fpDiskFS->GetDiskImg()->GetNumTracks() - 1)
		return;

	fTrack++;
	SetSpinner(IDC_DISKEDIT_TRACKSPIN, fTrack);
	LoadData();
}
