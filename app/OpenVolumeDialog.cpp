/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Allow the user to select a disk volume.
 */
#include "stdafx.h"
#include "OpenVolumeDialog.h"
#include "HelpTopics.h"
#include "Main.h"
#include "../diskimg/Win32Extra.h"	// need disk geometry calls
#include "../diskimg/ASPI.h"
//#include "resource.h"


BEGIN_MESSAGE_MAP(OpenVolumeDialog, CDialog)
	ON_COMMAND(IDHELP, OnHelp)
	//ON_NOTIFY(NM_CLICK, IDC_VOLUME_LIST, OnListClick)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_VOLUME_LIST, OnListChange)
	ON_NOTIFY(NM_DBLCLK, IDC_VOLUME_LIST, OnListDblClick)
	ON_CBN_SELCHANGE(IDC_VOLUME_FILTER, OnVolumeFilterSelChange)
END_MESSAGE_MAP()


/*
 * Set up the list of drives.
 */
BOOL
OpenVolumeDialog::OnInitDialog(void)
{
	CDialog::OnInitDialog();		// do any DDX init stuff
	const Preferences* pPreferences = GET_PREFERENCES();
	long defaultFilter;

	/* highlight/select entire line, not just filename */
	CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_VOLUME_LIST);
	ASSERT(pListView != nil);

	ListView_SetExtendedListViewStyleEx(pListView->m_hWnd,
		LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	/* disable the OK button until they click on something */
	CButton* pButton = (CButton*) GetDlgItem(IDOK);
	ASSERT(pButton != nil);

	pButton->EnableWindow(FALSE);

	/* if the read-only state is fixed, don't let them change it */
	if (!fAllowROChange) {
		CButton* pButton;
		pButton = (CButton*) GetDlgItem(IDC_OPENVOL_READONLY);
		ASSERT(pButton != nil);
		pButton->EnableWindow(FALSE);
	}

	/* prep the combo box */
	CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_VOLUME_FILTER);
	ASSERT(pCombo != nil);
	defaultFilter = pPreferences->GetPrefLong(kPrVolumeFilter);
	if (defaultFilter >= kBoth && defaultFilter <= kPhysical)
		pCombo->SetCurSel(defaultFilter);
	else {
		WMSG1("GLITCH: invalid defaultFilter in prefs (%d)\n", defaultFilter);
		pCombo->SetCurSel(kLogical);
	}

	/* two columns */
	CRect rect;
	pListView->GetClientRect(&rect);
	int width;

	width = pListView->GetStringWidth("XXVolume or Device NameXXmmmmmm");
	pListView->InsertColumn(0, "Volume or Device Name", LVCFMT_LEFT, width);
	pListView->InsertColumn(1, "Remarks", LVCFMT_LEFT,
		rect.Width() - width - ::GetSystemMetrics(SM_CXVSCROLL));

	// Load the drive list.
	LoadDriveList();

	// Kluge the physical drive 0 stuff.
	DiskImg::SetAllowWritePhys0(GET_PREFERENCES()->GetPrefBool(kPrOpenVolumePhys0));

	return TRUE;
}

/*
 * Convert values.
 */
void
OpenVolumeDialog::DoDataExchange(CDataExchange* pDX)
{
	DDX_Check(pDX, IDC_OPENVOL_READONLY, fReadOnly);
	WMSG1("DoDataExchange: fReadOnly==%d\n", fReadOnly);
}

/*
 * Load the set of logical and physical drives.
 */
void
OpenVolumeDialog::LoadDriveList(void)
{
	CWaitCursor waitc;
	CComboBox* pCombo;
	CListCtrl* pListView;
	int itemIndex = 0;
	int filterSelection;

	pListView = (CListCtrl*) GetDlgItem(IDC_VOLUME_LIST);
	ASSERT(pListView != nil);
	pCombo = (CComboBox*) GetDlgItem(IDC_VOLUME_FILTER);
	ASSERT(pCombo != nil);

	pListView->DeleteAllItems();

	/*
	 * Load the logical and physical drive sets as needed.  Do the "physical"
	 * set first because it's usually what we want.
	 */
	filterSelection = pCombo->GetCurSel();
	if (filterSelection == kPhysical || filterSelection == kBoth)
		LoadPhysicalDriveList(pListView, &itemIndex);
	if (filterSelection == kLogical || filterSelection == kBoth)
		LoadLogicalDriveList(pListView, &itemIndex);
}

/*
 * Determine the logical volumes available in the system and stuff them into
 * the list.
 */
bool
OpenVolumeDialog::LoadLogicalDriveList(CListCtrl* pListView, int* pItemIndex)
{
	DWORD drivesAvailable;
	bool isWin9x = IsWin9x();
	int itemIndex = *pItemIndex;

	ASSERT(pListView != nil);

	drivesAvailable = GetLogicalDrives();
	if (drivesAvailable == 0) {
		WMSG1("GetLogicalDrives failed, err=0x%08lx\n", drivesAvailable);
		return false;
	}
	WMSG1("GetLogicalDrives returned 0x%08lx\n", drivesAvailable);

	// SetErrorMode(SEM_FAILCRITICALERRORS)

	/* run through the list, from A-Z */
	int i;
	for (i = 0; i < kMaxLogicalDrives; i++) {
		fVolumeInfo[i].driveType = DRIVE_UNKNOWN;

		if ((drivesAvailable >> i) & 0x01) {
			char driveName[] = "_:\\";
			driveName[0] = 'A' + i;

			unsigned int driveType;
			const char* driveTypeComment = nil;
			BOOL result;

			driveType = fVolumeInfo[i].driveType = GetDriveType(driveName);
			switch (driveType) {
			case DRIVE_UNKNOWN:
				// The drive type cannot be determined.
				break;
			case DRIVE_NO_ROOT_DIR:
				// The root path is invalid. For example, no volume is mounted at the path.
				break;
			case DRIVE_REMOVABLE:
				// The disk can be removed from the drive.
				driveTypeComment = "Removable";
				break;
			case DRIVE_FIXED:
				// The disk cannot be removed from the drive.
				driveTypeComment = "Local Disk";
				break;
			case DRIVE_REMOTE:
				// The drive is a remote (network) drive.
				driveTypeComment = "Network";
				break;
			case DRIVE_CDROM:
				// The drive is a CD-ROM drive.
				driveTypeComment = "CD-ROM";
				break;
			case DRIVE_RAMDISK:
				// The drive is a RAM disk.
				break;
			default:
				WMSG1("UNKNOWN DRIVE TYPE %d\n", driveType);
				break;
			}

			if (driveType == DRIVE_CDROM && !DiskImgLib::Global::GetHasSPTI()) {
				/* use "physical" device via ASPI instead */
				WMSG1("Not including CD-ROM '%s' in logical drive list\n",
					driveName);
				continue;
			}

			char volNameBuf[256];
			char fsNameBuf[64];
			const char* errorComment = nil;
			//DWORD fsFlags;
			CString entryName, entryRemarks;

			result = ::GetVolumeInformation(driveName, volNameBuf,
				sizeof(volNameBuf), NULL, NULL, NULL /*&fsFlags*/, fsNameBuf,
				sizeof(fsNameBuf));
			if (result == FALSE) {
				DWORD err = GetLastError();
				if (err == ERROR_UNRECOGNIZED_VOLUME) {
					// Win2K: media exists but format not recognized
					errorComment = "Non-Windows format";
				} else if (err == ERROR_NOT_READY) {
					// Win2K: device exists but no media loaded
					if (isWin9x) {
						WMSG1("Not showing drive '%s': not ready\n",
							driveName);
						continue;	// safer not to show it
					} else
						errorComment = "Not ready";
				} else if (err == ERROR_PATH_NOT_FOUND /*Win2K*/ ||
						   err == ERROR_INVALID_DATA /*Win98*/)
				{
					// Win2K/Win98: device letter not in use
					WMSG1("GetVolumeInformation '%s': nothing there\n",
						driveName);
					continue;
				} else if (err == ERROR_INVALID_PARAMETER) {
					// Win2K: device is already open
					//WMSG1("GetVolumeInformation '%s': currently open??\n",
					//	driveName);
					errorComment = "(currently open?)";
					//continue;
				} else if (err == ERROR_ACCESS_DENIED) {
					// Win2K: disk is open no-read-sharing elsewhere
					errorComment = "(already open read-write)";
				} else if (err == ERROR_GEN_FAILURE) {
					// Win98: floppy format not recognzied
					// --> we don't want to access ProDOS floppies via A: in
					//     Win98, so we skip it here
					WMSG1("GetVolumeInformation '%s': general failure\n",
						driveName);
					continue;
				} else if (err == ERROR_INVALID_FUNCTION) {
					// Win2K: CD-ROM with HFS
					if (driveType == DRIVE_CDROM)
						errorComment = "Non-Windows format";
					else
						errorComment = "(invalid disc?)";
				} else {
					WMSG2("GetVolumeInformation '%s' failed: %ld\n",
						driveName, GetLastError());
					continue;
				}
				ASSERT(errorComment != nil);

				entryName.Format("(%c:)", 'A' + i);
				if (driveTypeComment != nil)
					entryRemarks.Format("%s - %s", driveTypeComment,
						errorComment);
				else
					entryRemarks.Format("%s", errorComment);
			} else {
				entryName.Format("%s (%c:)", volNameBuf, 'A' + i);
				if (driveTypeComment != nil)
					entryRemarks.Format("%s", driveTypeComment);
				else
					entryRemarks = "";
			}

			pListView->InsertItem(itemIndex, entryName);
			pListView->SetItemText(itemIndex, 1, entryRemarks);
			pListView->SetItemData(itemIndex, (DWORD) i + 'A');
//WMSG1("%%%% added logical %d\n", itemIndex);
			itemIndex++;
		} else {
			WMSG1(" (drive %c not available)\n", i + 'A');
		}
	}

	*pItemIndex = itemIndex;

	return true;
}

/*
 * Add a list of physical drives to the list control.
 *
 * I don't see a clever way to do this in Win2K except to open the first 8
 * or so devices and see what happens.
 *
 * Win9x isn't much better, though you can be reasonably confident that there
 * are at most 4 floppy drives and 4 hard drives.
 */
bool
OpenVolumeDialog::LoadPhysicalDriveList(CListCtrl* pListView, int* pItemIndex)
{
	bool isWin9x = IsWin9x();
	int itemIndex = *pItemIndex;
	int i;

	if (isWin9x) {
		// fairly arbitrary choices
		const int kMaxFloppies = 4;
		const int kMaxHardDrives = 8;

		for (i = 0; i < kMaxFloppies; i++) {
			CString driveName, remark;
			bool result;

			result = HasPhysicalDriveWin9x(i, &remark);
			if (result) {
				driveName.Format("Floppy disk %d", i);
				pListView->InsertItem(itemIndex, driveName);
				pListView->SetItemText(itemIndex, 1, remark);
				pListView->SetItemData(itemIndex, (DWORD) i);
//WMSG1("%%%% added floppy %d\n", itemIndex);
				itemIndex++;
			}
		}
		for (i = 0; i < kMaxHardDrives; i++) {
			CString driveName, remark;
			bool result;

			result = HasPhysicalDriveWin9x(i + 128, &remark);
			if (result) {
				driveName.Format("Hard drive %d", i);
				pListView->InsertItem(itemIndex, driveName);
				pListView->SetItemText(itemIndex, 1, remark);
				pListView->SetItemData(itemIndex, (DWORD) i + 128);
//WMSG1("%%%% added HD %d\n", itemIndex);
				itemIndex++;
			}
		}
	} else {
		for (i = 0; i < kMaxPhysicalDrives; i++) {
			CString driveName, remark;
			bool result;

			result = HasPhysicalDriveWin2K(i + 128, &remark);
			if (result) {
				driveName.Format("Physical disk %d", i);
				pListView->InsertItem(itemIndex, driveName);
				pListView->SetItemText(itemIndex, 1, remark);
				pListView->SetItemData(itemIndex, (DWORD) i + 128);	// HD volume
				itemIndex++;
			}
		}
	}

	if (DiskImgLib::Global::GetHasASPI()) {
		DIError dierr;
		DiskImgLib::ASPI* pASPI = DiskImgLib::Global::GetASPI();
		ASPIDevice* deviceArray = nil;
		int numDevices;

		dierr = pASPI->GetAccessibleDevices(
					ASPI::kDevMaskCDROM | ASPI::kDevMaskHardDrive,
					&deviceArray, &numDevices);
		if (dierr == kDIErrNone) {
			WMSG1("Adding %d ASPI CD-ROM devices\n", numDevices);
			for (i = 0; i < numDevices; i++) {
				CString driveName, remark;
				CString addr, vendor, product;
				DWORD aspiAddr;

				addr.Format("ASPI %d:%d:%d",
					deviceArray[i].GetAdapter(),
					deviceArray[i].GetTarget(),
					deviceArray[i].GetLun());
				vendor = deviceArray[i].GetVendorID();
				vendor.TrimRight();
				product = deviceArray[i].GetProductID();
				product.TrimRight();

				driveName.Format("%s %s", vendor, product);
				if (deviceArray[i].GetDeviceType() == ASPIDevice::kTypeCDROM)
					remark = "CD-ROM";
				else if (deviceArray[i].GetDeviceType() == ASPIDevice::kTypeDASD)
					remark = "Direct-access device";
				if (!deviceArray[i].GetDeviceReady())
					remark += " - Not ready";

				aspiAddr =	(DWORD) 0xaa << 24 |
							(DWORD) deviceArray[i].GetAdapter() << 16 |
							(DWORD) deviceArray[i].GetTarget() << 8 |
							(DWORD) deviceArray[i].GetLun();
				//WMSG2("ADDR for '%s' is 0x%08lx\n",
				//	(const char*) driveName, aspiAddr);

				pListView->InsertItem(itemIndex, driveName);
				pListView->SetItemText(itemIndex, 1, remark);
				pListView->SetItemData(itemIndex, aspiAddr);
				itemIndex++;
			}
		}

		delete[] deviceArray;
	}

	*pItemIndex = itemIndex;
	return true;
}

/*
 * Determine whether physical device N exists.
 *
 * Pass in the Int13 unit number, i.e. 0x00 for the first floppy drive.  Win9x
 * makes direct access to the hard drive very difficult, so we don't even try.
 */
bool
OpenVolumeDialog::HasPhysicalDriveWin9x(int unit, CString* pRemark)
{
	HANDLE handle = nil;
	const int VWIN32_DIOC_DOS_INT13 = 4;
	const int CARRY_FLAG = 1;
	BOOL result;
	typedef struct _DIOC_REGISTERS {
		DWORD reg_EBX;
		DWORD reg_EDX;
		DWORD reg_ECX;
		DWORD reg_EAX;
		DWORD reg_EDI;
		DWORD reg_ESI;
		DWORD reg_Flags;
	} DIOC_REGISTERS, *PDIOC_REGISTERS;
	DIOC_REGISTERS reg = {0};
	DWORD lastError, cb;
	unsigned char buf[512];

	if (unit > 4)
		return false;	// floppy drives only

	handle = CreateFile("\\\\.\\vwin32", 0, 0, NULL,
				OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		WMSG1(" Unable to open vwin32: %ld\n", ::GetLastError());
		return false;
	}

#if 0	// didn't do what I wanted
	reg.reg_EAX = MAKEWORD(0, 0x00);	// func 0x00 == reset controller
	reg.reg_EDX = MAKEWORD(unit, 0);	// specify driver
	result = DeviceIoControl(handle, VWIN32_DIOC_DOS_INT13, &reg,
				sizeof(reg), &reg, sizeof(reg), &cb, 0);
	WMSG3("  DriveReset(drive=0x%02x) result=%d carry=%d\n",
		unit, result, reg.reg_Flags & CARRY_FLAG);
#endif

	reg.reg_EAX = MAKEWORD(1, 0x02);	// read 1 sector
	reg.reg_EBX = (DWORD) buf;
	reg.reg_ECX = MAKEWORD(1, 0);		// sector 0 (+1), cylinder 0
	reg.reg_EDX = MAKEWORD(unit, 0);	// head

	result = DeviceIoControl(handle, VWIN32_DIOC_DOS_INT13, &reg,
				sizeof(reg), &reg, sizeof(reg), &cb, 0);
	lastError = GetLastError();
	::CloseHandle(handle);

	if (result == 0 || (reg.reg_Flags & CARRY_FLAG)) {
		int ah = HIBYTE(reg.reg_EAX);
		WMSG4(" DevIoCtrl(unit=%02xh) failed: result=%d lastErr=%d Flags=0x%08lx\n",
			unit, result, lastError, reg.reg_Flags);
		WMSG3("   AH=%d (EAX=0x%08lx) byte=0x%02x\n", ah, reg.reg_EAX, buf[0]);
		if (ah != 1) {
			// failure code 1 means "invalid parameter", drive doesn't exist
			// mine returns 128, "timeout", when no disk is in the drive
			*pRemark = "Not ready";
			return true;
		} else
			return false;
	}

	*pRemark = "Removable";

	return true;
}

/*
 * Determine whether physical device N exists.
 *
 * Pass in the Int13 unit number, i.e. 0x80 for the first hard drive.  This
 * should not be called with units for floppy drives (e.g. 0x00).
 */
bool
OpenVolumeDialog::HasPhysicalDriveWin2K(int unit, CString* pRemark)
{
	HANDLE hDevice;			// handle to the drive to be examined 
	DISK_GEOMETRY dg;		// disk drive geometry structure
	DISK_GEOMETRY_EX dge;	// extended geometry request buffer
	BOOL result;			// results flag
	DWORD junk;				// discard results
	LONGLONG diskSize;		// size of the drive, in bytes
	CString fileName;
	DWORD err;

	/*
	 * See if the drive is there.
	 */
	ASSERT(unit >= 128 && unit < 160);		// arbitrary max
	fileName.Format("\\\\.\\PhysicalDrive%d", unit - 128);

	hDevice = ::CreateFile((const char*) fileName,	// drive to open
				0,					// no access to the drive
				FILE_SHARE_READ | FILE_SHARE_WRITE,	// share mode
				NULL,				// default security attributes
				OPEN_EXISTING,		// disposition
				0,					// file attributes
				NULL);				// do not copy file attributes

	if (hDevice == INVALID_HANDLE_VALUE) // cannot open the drive
		return false;

	/*
	 * Try to get the drive geometry.  First try with the fancy WinXP call,
	 * then fall back to the Win2K call if it doesn't exist.
	 */
	result = ::DeviceIoControl(hDevice,
				IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
				NULL, 0,				// input buffer
				&dge, sizeof(dge),		// output buffer
				&junk,					// # bytes returned
				(LPOVERLAPPED) NULL);	// synchronous I/O
	if (result) {
		diskSize = dge.DiskSize.QuadPart;
		WMSG1(" EX results for device %02xh\n", unit);
		WMSG2("  Disk size = %I64d (bytes) = %I64d (MB)\n",
			diskSize, diskSize / (1024*1024));
		if (diskSize > 1024*1024*1024)
			pRemark->Format("Size is %.2fGB",
				(double) diskSize / (1024.0 * 1024.0 * 1024.0));
		else
			pRemark->Format("Size is %.2fMB",
				(double) diskSize / (1024.0 * 1024.0));
	} else {
		// Win2K shows ERROR_INVALID_FUNCTION or ERROR_NOT_SUPPORTED
		WMSG1("IOCTL_DISK_GET_DRIVE_GEOMETRY_EX failed, error was %ld\n",
			GetLastError());
		result = ::DeviceIoControl(hDevice,	// device to be queried
					IOCTL_DISK_GET_DRIVE_GEOMETRY,	// operation to perform
					NULL, 0,				// no input buffer
					&dg, sizeof(dg),		// output buffer
					&junk,					// # bytes returned
					(LPOVERLAPPED) NULL);	// synchronous I/O

		if (result) {
			WMSG1(" Results for device %02xh\n", unit);
			WMSG1("  Cylinders = %I64d\n", dg.Cylinders);
			WMSG1("  Tracks per cylinder = %ld\n", (ULONG) dg.TracksPerCylinder);
			WMSG1("  Sectors per track = %ld\n", (ULONG) dg.SectorsPerTrack);
			WMSG1("  Bytes per sector = %ld\n", (ULONG) dg.BytesPerSector);

			diskSize = dg.Cylinders.QuadPart * (ULONG)dg.TracksPerCylinder *
						(ULONG)dg.SectorsPerTrack * (ULONG)dg.BytesPerSector;
			WMSG2("Disk size = %I64d (bytes) = %I64d (MB)\n", diskSize,
					diskSize / (1024 * 1024));
			if (diskSize > 1024*1024*1024)
				pRemark->Format("Size is %.2fGB",
					(double) diskSize / (1024.0 * 1024.0 * 1024.0));
			else
				pRemark->Format("Size is %.2fMB",
					(double) diskSize / (1024.0 * 1024.0));
		} else {
			err = GetLastError();
		}
	}

	::CloseHandle(hDevice);

	if (!result) {
		WMSG1("DeviceIoControl(IOCTL_DISK_GET_DRIVE_GEOMETRY) failed (err=%ld)\n",
			err);
		*pRemark = "Not ready";
	}

	return true;
}


/*
 * Something changed in the list.  Update the "OK" button.
 */
void
OpenVolumeDialog::OnListChange(NMHDR*, LRESULT* pResult)
{
	CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_VOLUME_LIST);
	CButton* pButton = (CButton*) GetDlgItem(IDOK);
	pButton->EnableWindow(pListView->GetSelectedCount() != 0);
	//WMSG1("ENABLE %d\n", pListView->GetSelectedCount() != 0);

	*pResult = 0;
}

/*
 * Double click.
 */
void
OpenVolumeDialog::OnListDblClick(NMHDR* pNotifyStruct, LRESULT* pResult)
{
	CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_VOLUME_LIST);
	CButton* pButton = (CButton*) GetDlgItem(IDOK);

	if (pListView->GetSelectedCount() != 0) {
		pButton->EnableWindow();
		OnOK();
	}

	*pResult = 0;
}

/*
 * The volume filter drop-down box has changed.
 */
void
OpenVolumeDialog::OnVolumeFilterSelChange(void)
{
	CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_VOLUME_FILTER);
	ASSERT(pCombo != nil);
	WMSG1("+++ SELECTION IS NOW %d\n", pCombo->GetCurSel());
	LoadDriveList();
}

/*
 * Verify their selection.
 */
void
OpenVolumeDialog::OnOK(void)
{
	/*
	 * Figure out the (zero-based) drive letter.
	 */
	CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_VOLUME_LIST);
	ASSERT(pListView != nil);

	if (pListView->GetSelectedCount() != 1) {
		CString msg, failed;
		failed.LoadString(IDS_FAILED);
		msg.LoadString(IDS_VOLUME_SELECT_ONE);
		MessageBox(msg, failed, MB_OK);
		return;
	}

	POSITION posn;
	posn = pListView->GetFirstSelectedItemPosition();
	if (posn == nil) {
		ASSERT(false);
		return;
	}
	int num = pListView->GetNextSelectedItem(posn);
	DWORD driveID = pListView->GetItemData(num);
	UINT formatID = 0;

	if (HIBYTE(HIWORD(driveID)) == 0xaa) {
		fChosenDrive.Format("%s%d:%d:%d\\",
			DiskImgLib::kASPIDev,
			LOBYTE(HIWORD(driveID)),
			HIBYTE(LOWORD(driveID)),
			LOBYTE(LOWORD(driveID)));
		//ForceReadOnly(true);
	} else if (driveID >= 'A' && driveID <= 'Z') {
		/*
		 * Do we want to let them do this?  We show some logical drives
		 * that we don't want them to actually use.
		 */
		switch (fVolumeInfo[driveID-'A'].driveType) {
		case DRIVE_REMOVABLE:
		case DRIVE_FIXED:
			break;				// allow
		case DRIVE_CDROM:		//formatID = IDS_VOLUME_NO_CDROM;
			ForceReadOnly(true);
			break;
		case DRIVE_REMOTE:		formatID = IDS_VOLUME_NO_REMOTE;
			break;
		case DRIVE_RAMDISK:		formatID = IDS_VOLUME_NO_RAMDISK;
			break;
		case DRIVE_UNKNOWN:
		case DRIVE_NO_ROOT_DIR:
		default:				formatID = IDS_VOLUME_NO_GENERIC;
			break;
		}

		fChosenDrive.Format("%c:\\", driveID);
	} else if ((driveID >= 0 && driveID < 4) ||
			   (driveID >= 0x80 && driveID < 0x88))
	{
		fChosenDrive.Format("%02x:\\", driveID);
	} else {
		ASSERT(false);
		return;
	}

	if (formatID != 0) {
		CString msg, notAllowed;

		notAllowed.LoadString(IDS_NOT_ALLOWED);
		msg.LoadString(formatID);
		MessageBox(msg, notAllowed, MB_OK);
	} else {
		Preferences* pPreferences = GET_PREFERENCES_WR();
		CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_VOLUME_FILTER);
		pPreferences->SetPrefLong(kPrVolumeFilter, pCombo->GetCurSel());
		WMSG1("SETTING PREF TO %ld\n", pCombo->GetCurSel());

		CDialog::OnOK();
	}
}


/*
 * User pressed the "Help" button.
 */
void
OpenVolumeDialog::OnHelp(void)
{
	WinHelp(HELP_TOPIC_OPEN_VOLUME, HELP_CONTEXT);
}


/*
 * Set the state of the "read only" checkbox in the dialog.
 */
void
OpenVolumeDialog::ForceReadOnly(bool readOnly) const
{
	CButton* pButton = (CButton*) GetDlgItem(IDC_OPENVOL_READONLY);
	ASSERT(pButton != nil);

	if (readOnly)
		pButton->SetCheck(BST_CHECKED);
	else
		pButton->SetCheck(BST_UNCHECKED);
	WMSG1("FORCED READ ONLY %d\n", readOnly);
}
