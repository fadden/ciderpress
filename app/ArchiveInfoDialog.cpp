/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of the various ArchiveInfo dialog classes.
 */
#include "StdAfx.h"
#include "HelpTopics.h"
#include "ArchiveInfoDialog.h"
#include "../prebuilt/NufxLib.h"

/*
 * ===========================================================================
 *		ArchiveInfoDialog
 * ===========================================================================
 */

BEGIN_MESSAGE_MAP(ArchiveInfoDialog, CDialog)
	ON_COMMAND(IDHELP, OnHelp)
END_MESSAGE_MAP()

/*
 * Show general help for the archive info dialogs.
 */
void
ArchiveInfoDialog::OnHelp(void)
{
	WinHelp(HELP_TOPIC_ARCHIVE_INFO, HELP_CONTEXT);
}


/*
 * ===========================================================================
 *		NufxArchiveInfoDialog
 * ===========================================================================
 */

 /*
 * Set up fields with NuFX archive info.
 */
BOOL
NufxArchiveInfoDialog::OnInitDialog(void)
{
	CString notAvailable = "(not available)";
	NuArchive* pNuArchive;
	const NuMasterHeader* pMasterHeader;
	CWnd* pWnd;
	CString tmpStr;
	NuAttr attr;
	NuError nerr;
	time_t when;

	ASSERT(fpArchive != nil);

	pNuArchive = fpArchive->GetNuArchivePointer();
	ASSERT(pNuArchive != nil);
	(void) NuGetMasterHeader(pNuArchive, &pMasterHeader);
	ASSERT(pMasterHeader != nil);

	pWnd = GetDlgItem(IDC_AI_FILENAME);
	pWnd->SetWindowText(fpArchive->GetPathName());

	pWnd = GetDlgItem(IDC_AINUFX_RECORDS);
	nerr = NuGetAttr(pNuArchive, kNuAttrNumRecords, &attr);
	if (nerr == kNuErrNone)
		tmpStr.Format("%ld", attr);
	else
		tmpStr = notAvailable;
	pWnd->SetWindowText(tmpStr);

	pWnd = GetDlgItem(IDC_AINUFX_FORMAT);
	nerr = NuGetAttr(pNuArchive, kNuAttrArchiveType, &attr);
	switch (attr) {
    case kNuArchiveNuFX:			tmpStr = "NuFX";					break;
    case kNuArchiveNuFXInBNY:		tmpStr = "NuFX in Binary II";		break;
    case kNuArchiveNuFXSelfEx:		tmpStr = "Self-extracting NuFX";	break;
    case kNuArchiveNuFXSelfExInBNY:	tmpStr = "Self-extracting NuFX in Binary II";
		break;
    case kNuArchiveBNY:				tmpStr = "Binary II";				break;
	default:
		tmpStr = "(unknown)";
		break;
	};
	pWnd->SetWindowText(tmpStr);

	pWnd = GetDlgItem(IDC_AINUFX_MASTERVERSION);
	tmpStr.Format("%ld", pMasterHeader->mhMasterVersion);
	pWnd->SetWindowText(tmpStr);

	pWnd = GetDlgItem(IDC_AINUFX_CREATEWHEN);
	when = NufxArchive::DateTimeToSeconds(&pMasterHeader->mhArchiveCreateWhen);
	tmpStr.Format("%.24s", ctime(&when));
	pWnd->SetWindowText(tmpStr);

	pWnd = GetDlgItem(IDC_AINUFX_MODIFYWHEN);
	when = NufxArchive::DateTimeToSeconds(&pMasterHeader->mhArchiveModWhen);
	tmpStr.Format("%.24s", ctime(&when));
	pWnd->SetWindowText(tmpStr);

	pWnd = GetDlgItem(IDC_AINUFX_JUNKSKIPPED);
	nerr = NuGetAttr(pNuArchive, kNuAttrJunkOffset, &attr);
	if (nerr == kNuErrNone)
		tmpStr.Format("%ld bytes", attr);
	else
		tmpStr = notAvailable;
	pWnd->SetWindowText(tmpStr);

	return ArchiveInfoDialog::OnInitDialog();
}


/*
 * ===========================================================================
 *		DiskArchiveInfoDialog
 * ===========================================================================
 */

BEGIN_MESSAGE_MAP(DiskArchiveInfoDialog, ArchiveInfoDialog)
	ON_CBN_SELCHANGE(IDC_AIDISK_SUBVOLSEL, OnSubVolSelChange)
END_MESSAGE_MAP()

/*
 * Set up fields with disk archive info.
 */
BOOL
DiskArchiveInfoDialog::OnInitDialog(void)
{
	CWnd* pWnd;
	CString tmpStr;
	const DiskImg* pDiskImg;
	const DiskFS* pDiskFS;

	ASSERT(fpArchive != nil);

	pDiskImg = fpArchive->GetDiskImg();
	ASSERT(pDiskImg != nil);
	pDiskFS = fpArchive->GetDiskFS();
	ASSERT(pDiskFS != nil);

	/*
	 * Volume characteristics.
	 */
	pWnd = GetDlgItem(IDC_AI_FILENAME);
	pWnd->SetWindowText(fpArchive->GetPathName());

	pWnd = GetDlgItem(IDC_AIDISK_OUTERFORMAT);
	pWnd->SetWindowText(DiskImg::ToString(pDiskImg->GetOuterFormat()));

	pWnd = GetDlgItem(IDC_AIDISK_FILEFORMAT);
	pWnd->SetWindowText(DiskImg::ToString(pDiskImg->GetFileFormat()));

	pWnd = GetDlgItem(IDC_AIDISK_PHYSICALFORMAT);
	DiskImg::PhysicalFormat physicalFormat = pDiskImg->GetPhysicalFormat();
	if (physicalFormat == DiskImg::kPhysicalFormatNib525_6656 ||
		physicalFormat == DiskImg::kPhysicalFormatNib525_6384 ||
		physicalFormat == DiskImg::kPhysicalFormatNib525_Var)
	{
		CString tmpStr;
		const DiskImg::NibbleDescr* pNibbleDescr = pDiskImg->GetNibbleDescr();
		if (pNibbleDescr != nil)
			tmpStr.Format("%s, layout is \"%s\"",
				DiskImg::ToString(physicalFormat), pNibbleDescr->description);
		else
			tmpStr = DiskImg::ToString(physicalFormat);	// unexpected
		pWnd->SetWindowText(tmpStr);
	} else {
		pWnd->SetWindowText(DiskImg::ToString(physicalFormat));
	}

	FillInVolumeInfo(pDiskFS);

	/*
	 * Configure the sub-volume drop down menu.  If there's only one item,
	 * we disable it.
	 */
	CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_AIDISK_SUBVOLSEL);
	int idx = 0;

	AddSubVolumes(pDiskFS, "", &idx);
	ASSERT(idx > 0);		// must have at least the top-level DiskFS

	pCombo->SetCurSel(0);
	if (idx == 1)
		pCombo->EnableWindow(FALSE);

	return ArchiveInfoDialog::OnInitDialog();
}

/*
 * Recursively add sub-volumes to the list.
 */
void
DiskArchiveInfoDialog::AddSubVolumes(const DiskFS* pDiskFS, const char* prefix,
	int* pIdx)
{
	CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_AIDISK_SUBVOLSEL);
	CString tmpStr;

	/*
	 * Add the current DiskFS.
	 */
	tmpStr = prefix;
	tmpStr += pDiskFS->GetVolumeID();
	pCombo->AddString(tmpStr);
	pCombo->SetItemData(*pIdx, (unsigned long) pDiskFS);
	(*pIdx)++;

	/*
	 * Add everything beneath the current level.
	 */
	DiskFS::SubVolume* pSubVol;
	pSubVol = pDiskFS->GetNextSubVolume(nil);
	tmpStr = prefix;
	tmpStr += "   ";
	while (pSubVol != nil) {
		AddSubVolumes(pSubVol->GetDiskFS(), tmpStr, pIdx);

		pSubVol = pDiskFS->GetNextSubVolume(pSubVol);
	}
}

/*
 * The user has changed their selection in the sub-volume pulldown menu.
 */
void
DiskArchiveInfoDialog::OnSubVolSelChange(void)
{
	CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_AIDISK_SUBVOLSEL);
	ASSERT(pCombo != nil);
	//WMSG1("+++ SELECTION IS NOW %d\n", pCombo->GetCurSel());

	const DiskFS* pDiskFS;
	pDiskFS = (DiskFS*) pCombo->GetItemData(pCombo->GetCurSel());
	ASSERT(pDiskFS != nil);
	FillInVolumeInfo(pDiskFS);
}

/*
 * Fill in the volume-specific info fields.
 */
void
DiskArchiveInfoDialog::FillInVolumeInfo(const DiskFS* pDiskFS)
{
	const DiskImg* pDiskImg = pDiskFS->GetDiskImg();
	CString unknown = "(unknown)";
	CString tmpStr;
	DIError dierr;
	CWnd* pWnd;

	pWnd = GetDlgItem(IDC_AIDISK_SECTORORDER);
	pWnd->SetWindowText(DiskImg::ToString(pDiskImg->GetSectorOrder()));

	pWnd = GetDlgItem(IDC_AIDISK_FSFORMAT);
	pWnd->SetWindowText(DiskImg::ToString(pDiskImg->GetFSFormat()));

	pWnd = GetDlgItem(IDC_AIDISK_FILECOUNT);
	tmpStr.Format("%ld", pDiskFS->GetFileCount());
	pWnd->SetWindowText(tmpStr);

	long totalUnits, freeUnits;
	int unitSize;
	CString reducedSize;

	dierr = pDiskFS->GetFreeSpaceCount(&totalUnits, &freeUnits, &unitSize);
	if (dierr == kDIErrNone) {

		/* got the space; break it down by disk type */
		if (unitSize == DiskImgLib::kBlockSize) {
			pWnd = GetDlgItem(IDC_AIDISK_CAPACITY);
			GetReducedSize(totalUnits, unitSize, &reducedSize);
			tmpStr.Format("%ld blocks (%s)",
				totalUnits, reducedSize);
			if (totalUnits != pDiskImg->GetNumBlocks()) {
				CString tmpStr2;
				tmpStr2.Format(", image has room for %ld blocks",
					pDiskImg->GetNumBlocks());
				tmpStr += tmpStr2;
			}
			pWnd->SetWindowText(tmpStr);

			pWnd = GetDlgItem(IDC_AIDISK_FREESPACE);
			GetReducedSize(freeUnits, unitSize, &reducedSize);
			tmpStr.Format("%ld blocks (%s)",
				freeUnits, reducedSize);
			pWnd->SetWindowText(tmpStr);
		} else {
			ASSERT(unitSize == DiskImgLib::kSectorSize);

			pWnd = GetDlgItem(IDC_AIDISK_CAPACITY);
			GetReducedSize(totalUnits, unitSize, &reducedSize);
			tmpStr.Format("%ld sectors (%s)",
				totalUnits, reducedSize);
			pWnd->SetWindowText(tmpStr);

			pWnd = GetDlgItem(IDC_AIDISK_FREESPACE);
			GetReducedSize(freeUnits, unitSize, &reducedSize);
			tmpStr.Format("%ld sectors (%s)",
				freeUnits, reducedSize);
			pWnd->SetWindowText(tmpStr);
		}
	} else {
		/* "free space" not supported; fill in what we do know */
		pWnd = GetDlgItem(IDC_AIDISK_CAPACITY);
		if (pDiskImg->GetHasBlocks()) {
			totalUnits = pDiskImg->GetNumBlocks();
			GetReducedSize(totalUnits, DiskImgLib::kBlockSize, &reducedSize);
			tmpStr.Format("%ld blocks (%s)",
				totalUnits, reducedSize);
		} else if (pDiskImg->GetHasSectors()) {
			tmpStr.Format("%ld tracks, %d sectors per track",
				pDiskImg->GetNumTracks(), pDiskImg->GetNumSectPerTrack());
		} else {
			tmpStr = unknown;
		}
		pWnd->SetWindowText(tmpStr);

		pWnd = GetDlgItem(IDC_AIDISK_FREESPACE);
		pWnd->SetWindowText(unknown);
	}

	pWnd = GetDlgItem(IDC_AIDISK_WRITEABLE);
	tmpStr = pDiskFS->GetReadWriteSupported() ? "Yes" : "No";
	pWnd->SetWindowText(tmpStr);

	pWnd = GetDlgItem(IDC_AIDISK_DAMAGED);
	tmpStr = pDiskFS->GetFSDamaged() ? "Yes" : "No";
	pWnd->SetWindowText(tmpStr);

	const char* cp;
	char* outp;

	pWnd = GetDlgItem(IDC_AIDISK_NOTES);
	cp = pDiskImg->GetNotes();
	outp = tmpStr.GetBuffer(strlen(cp) * 2 +1);
	/* convert '\n' to '\r\n' */
	while (*cp != '\0') {
		if (*cp == '\n')
			*outp++ = '\r';
		*outp++ = *cp++;
	}
	*outp = '\0';
	tmpStr.ReleaseBuffer();
	/* drop the trailing linefeed */
	if (!tmpStr.IsEmpty() && tmpStr.GetAt(tmpStr.GetLength()-1) == '\n')
		tmpStr.TrimRight();		// trim the whitespace chars off
	pWnd->SetWindowText(tmpStr);
}

/*
 * Reduce a size to something meaningful (KB, MB, GB).
 */
void
DiskArchiveInfoDialog::GetReducedSize(long numUnits, int unitSize,
	CString* pOut) const
{
	LONGLONG sizeInBytes = numUnits;
	sizeInBytes *= unitSize;
	long reducedSize;

	if (sizeInBytes < 0) {
		ASSERT(false);
		pOut->Format("<bogus>");
		return;
	}

	if (sizeInBytes >= 1024*1024*1024) {
		reducedSize = (long) (sizeInBytes / (1024*1024));
		pOut->Format("%.2fGB", reducedSize / 1024.0);
	} else if (sizeInBytes >= 1024*1024) {
		reducedSize = (long) (sizeInBytes / 1024);
		pOut->Format("%.2fMB", reducedSize / 1024.0);
	} else {
		pOut->Format("%.2fKB", ((long) sizeInBytes) / 1024.0);
	}
}


/*
 * ===========================================================================
 *		BnyArchiveInfoDialog
 * ===========================================================================
 */

/*
 * Set up fields with Binary II info.
 *
 * Binary II files are pretty dull.
 */
BOOL
BnyArchiveInfoDialog::OnInitDialog(void)
{
	CWnd* pWnd;
	CString tmpStr;

	ASSERT(fpArchive != nil);

	pWnd = GetDlgItem(IDC_AI_FILENAME);
	pWnd->SetWindowText(fpArchive->GetPathName());
	tmpStr.Format("%ld", fpArchive->GetNumEntries());
	pWnd = GetDlgItem(IDC_AIBNY_RECORDS);
	pWnd->SetWindowText(tmpStr);

	return ArchiveInfoDialog::OnInitDialog();
}


/*
 * ===========================================================================
 *		AcuArchiveInfoDialog
 * ===========================================================================
 */

/*
 * Set up fields with ACU info.
 */
BOOL
AcuArchiveInfoDialog::OnInitDialog(void)
{
	CWnd* pWnd;
	CString tmpStr;

	ASSERT(fpArchive != nil);

	pWnd = GetDlgItem(IDC_AI_FILENAME);
	pWnd->SetWindowText(fpArchive->GetPathName());
	tmpStr.Format("%ld", fpArchive->GetNumEntries());
	pWnd = GetDlgItem(IDC_AIBNY_RECORDS);
	pWnd->SetWindowText(tmpStr);

	return ArchiveInfoDialog::OnInitDialog();
}
