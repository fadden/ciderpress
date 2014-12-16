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
#include "Main.h"
#include "../diskimg/Win32Extra.h"  // need disk geometry calls
#include "../diskimg/ASPI.h"


BEGIN_MESSAGE_MAP(OpenVolumeDialog, CDialog)
    ON_COMMAND(IDHELP, OnHelp)
    //ON_NOTIFY(NM_CLICK, IDC_VOLUME_LIST, OnListClick)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_VOLUME_LIST, OnListChange)
    ON_NOTIFY(NM_DBLCLK, IDC_VOLUME_LIST, OnListDblClick)
    ON_CBN_SELCHANGE(IDC_VOLUME_FILTER, OnVolumeFilterSelChange)
END_MESSAGE_MAP()


BOOL OpenVolumeDialog::OnInitDialog(void)
{
    /*
     * Sets up the list of drives.
     */

    CDialog::OnInitDialog();        // do any DDX init stuff
    const Preferences* pPreferences = GET_PREFERENCES();
    long defaultFilter;

    /* highlight/select entire line, not just filename */
    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_VOLUME_LIST);
    ASSERT(pListView != NULL);

    ListView_SetExtendedListViewStyleEx(pListView->m_hWnd,
        LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

    /* disable the OK button until they click on something */
    CButton* pButton = (CButton*) GetDlgItem(IDOK);
    ASSERT(pButton != NULL);

    pButton->EnableWindow(FALSE);

    /* if the read-only state is fixed, don't let them change it */
    if (!fAllowROChange) {
        CButton* pButton;
        pButton = (CButton*) GetDlgItem(IDC_OPENVOL_READONLY);
        ASSERT(pButton != NULL);
        pButton->EnableWindow(FALSE);
    }

    /* prep the combo box */
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_VOLUME_FILTER);
    ASSERT(pCombo != NULL);
    defaultFilter = pPreferences->GetPrefLong(kPrVolumeFilter);
    if (defaultFilter >= kBoth && defaultFilter <= kPhysical)
        pCombo->SetCurSel(defaultFilter);
    else {
        LOGI("GLITCH: invalid defaultFilter in prefs (%d)", defaultFilter);
        pCombo->SetCurSel(kLogical);
    }

    /* two columns */
    CRect rect;
    pListView->GetClientRect(&rect);
    int width;

    width = pListView->GetStringWidth(L"XXVolume or Device NameXXmmmmmm");
    pListView->InsertColumn(0, L"Volume or Device Name", LVCFMT_LEFT, width);
    pListView->InsertColumn(1, L"Remarks", LVCFMT_LEFT,
        rect.Width() - width - ::GetSystemMetrics(SM_CXVSCROLL));

    // Load the drive list.
    LoadDriveList();

    // Kluge the physical drive 0 stuff.
    DiskImg::SetAllowWritePhys0(GET_PREFERENCES()->GetPrefBool(kPrOpenVolumePhys0));

    return TRUE;
}

void OpenVolumeDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Check(pDX, IDC_OPENVOL_READONLY, fReadOnly);
    LOGI("DoDataExchange: fReadOnly==%d", fReadOnly);
}

void OpenVolumeDialog::LoadDriveList(void)
{
    CWaitCursor waitc;
    CComboBox* pCombo;
    CListCtrl* pListView;
    int itemIndex = 0;
    int filterSelection;

    pListView = (CListCtrl*) GetDlgItem(IDC_VOLUME_LIST);
    ASSERT(pListView != NULL);
    pCombo = (CComboBox*) GetDlgItem(IDC_VOLUME_FILTER);
    ASSERT(pCombo != NULL);

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

bool OpenVolumeDialog::LoadLogicalDriveList(CListCtrl* pListView, int* pItemIndex)
{
    DWORD drivesAvailable;
    bool isWin9x = IsWin9x();
    int itemIndex = *pItemIndex;

    ASSERT(pListView != NULL);

    drivesAvailable = GetLogicalDrives();
    if (drivesAvailable == 0) {
        LOGI("GetLogicalDrives failed, err=0x%08lx", drivesAvailable);
        return false;
    }
    LOGI("GetLogicalDrives returned 0x%08lx", drivesAvailable);

    // SetErrorMode(SEM_FAILCRITICALERRORS)

    /* run through the list, from A-Z */
    int i;
    for (i = 0; i < kMaxLogicalDrives; i++) {
        fVolumeInfo[i].driveType = DRIVE_UNKNOWN;

        if ((drivesAvailable >> i) & 0x01) {
            WCHAR driveName[] = L"_:\\";
            driveName[0] = 'A' + i;

            unsigned int driveType;
            const WCHAR* driveTypeComment = NULL;
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
                driveTypeComment = L"Removable";
                break;
            case DRIVE_FIXED:
                // The disk cannot be removed from the drive.
                driveTypeComment = L"Local Disk";
                break;
            case DRIVE_REMOTE:
                // The drive is a remote (network) drive.
                driveTypeComment = L"Network";
                break;
            case DRIVE_CDROM:
                // The drive is a CD-ROM drive.
                driveTypeComment = L"CD-ROM";
                break;
            case DRIVE_RAMDISK:
                // The drive is a RAM disk.
                break;
            default:
                LOGI("UNKNOWN DRIVE TYPE %d", driveType);
                break;
            }

            if (driveType == DRIVE_CDROM && !DiskImgLib::Global::GetHasSPTI()) {
                /* use "physical" device via ASPI instead */
                LOGI("Not including CD-ROM '%ls' in logical drive list",
                    driveName);
                continue;
            }

            WCHAR volNameBuf[256];
            WCHAR fsNameBuf[64];
            const WCHAR* errorComment = NULL;
            //DWORD fsFlags;
            CString entryName, entryRemarks;

            result = ::GetVolumeInformation(driveName, volNameBuf,
                NELEM(volNameBuf), NULL, NULL, NULL /*&fsFlags*/, fsNameBuf,
                NELEM(fsNameBuf));
            if (result == FALSE) {
                DWORD err = GetLastError();
                if (err == ERROR_UNRECOGNIZED_VOLUME) {
                    // Win2K: media exists but format not recognized
                    errorComment = L"Non-Windows format";
                } else if (err == ERROR_NOT_READY) {
                    // Win2K: device exists but no media loaded
                    if (isWin9x) {
                        LOGI("Not showing drive '%ls': not ready",
                            driveName);
                        continue;   // safer not to show it
                    } else
                        errorComment = L"Not ready";
                } else if (err == ERROR_PATH_NOT_FOUND /*Win2K*/ ||
                           err == ERROR_INVALID_DATA /*Win98*/)
                {
                    // Win2K/Win98: device letter not in use
                    LOGI("GetVolumeInformation '%ls': nothing there",
                        driveName);
                    continue;
                } else if (err == ERROR_INVALID_PARAMETER) {
                    // Win2K: device is already open
                    //LOGI("GetVolumeInformation '%ls': currently open??",
                    //  driveName);
                    errorComment = L"(currently open?)";
                    //continue;
                } else if (err == ERROR_ACCESS_DENIED) {
                    // Win2K: disk is open no-read-sharing elsewhere
                    errorComment = L"(already open read-write)";
                } else if (err == ERROR_GEN_FAILURE) {
                    // Win98: floppy format not recognzied
                    // --> we don't want to access ProDOS floppies via A: in
                    //     Win98, so we skip it here
                    LOGI("GetVolumeInformation '%ls': general failure",
                        driveName);
                    continue;
                } else if (err == ERROR_INVALID_FUNCTION) {
                    // Win2K: CD-ROM with HFS
                    if (driveType == DRIVE_CDROM)
                        errorComment = L"Non-Windows format";
                    else
                        errorComment = L"(invalid disc?)";
                } else {
                    LOGI("GetVolumeInformation '%ls' failed: %ld",
                        driveName, GetLastError());
                    continue;
                }
                ASSERT(errorComment != NULL);

                entryName.Format(L"(%c:)", 'A' + i);
                if (driveTypeComment != NULL)
                    entryRemarks.Format(L"%ls - %ls", driveTypeComment,
                        errorComment);
                else
                    entryRemarks.Format(L"%ls", errorComment);
            } else {
                entryName.Format(L"%ls (%c:)", volNameBuf, 'A' + i);
                if (driveTypeComment != NULL)
                    entryRemarks.Format(L"%ls", driveTypeComment);
                else
                    entryRemarks = "";
            }

            pListView->InsertItem(itemIndex, entryName);
            pListView->SetItemText(itemIndex, 1, entryRemarks);
            pListView->SetItemData(itemIndex, (DWORD) i + 'A');
//LOGI("%%%% added logical %d", itemIndex);
            itemIndex++;
        } else {
            LOGI(" (drive %c not available)", i + 'A');
        }
    }

    *pItemIndex = itemIndex;

    return true;
}

bool OpenVolumeDialog::LoadPhysicalDriveList(CListCtrl* pListView, int* pItemIndex)
{
    /*
     * I don't see a clever way to do this in Win2K except to open the first 8
     * or so devices and see what happens.
     *
     * Win9x isn't much better, though you can be reasonably confident that there
     * are at most 4 floppy drives and 4 hard drives.
     *
     * TODO: check again for a better way
     */

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
                driveName.Format(L"Floppy disk %d", i);
                pListView->InsertItem(itemIndex, driveName);
                pListView->SetItemText(itemIndex, 1, remark);
                pListView->SetItemData(itemIndex, (DWORD) i);
//LOGI("%%%% added floppy %d", itemIndex);
                itemIndex++;
            }
        }
        for (i = 0; i < kMaxHardDrives; i++) {
            CString driveName, remark;
            bool result;

            result = HasPhysicalDriveWin9x(i + 128, &remark);
            if (result) {
                driveName.Format(L"Hard drive %d", i);
                pListView->InsertItem(itemIndex, driveName);
                pListView->SetItemText(itemIndex, 1, remark);
                pListView->SetItemData(itemIndex, (DWORD) i + 128);
//LOGI("%%%% added HD %d", itemIndex);
                itemIndex++;
            }
        }
    } else {
        for (i = 0; i < kMaxPhysicalDrives; i++) {
            CString driveName, remark;
            bool result;

            result = HasPhysicalDriveWin2K(i + 128, &remark);
            if (result) {
                driveName.Format(L"Physical disk %d", i);
                pListView->InsertItem(itemIndex, driveName);
                pListView->SetItemText(itemIndex, 1, remark);
                pListView->SetItemData(itemIndex, (DWORD) i + 128); // HD volume
                itemIndex++;
            }
        }
    }

    if (DiskImgLib::Global::GetHasASPI()) {
        LOGI("IGNORING ASPI");
#if 0   // can we remove this?
        DIError dierr;
        DiskImgLib::ASPI* pASPI = DiskImgLib::Global::GetASPI();
        ASPIDevice* deviceArray = NULL;
        int numDevices;

        dierr = pASPI->GetAccessibleDevices(
                    ASPI::kDevMaskCDROM | ASPI::kDevMaskHardDrive,
                    &deviceArray, &numDevices);
        if (dierr == kDIErrNone) {
            LOGI("Adding %d ASPI CD-ROM devices", numDevices);
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

                aspiAddr =  (DWORD) 0xaa << 24 |
                            (DWORD) deviceArray[i].GetAdapter() << 16 |
                            (DWORD) deviceArray[i].GetTarget() << 8 |
                            (DWORD) deviceArray[i].GetLun();
                //LOGI("ADDR for '%s' is 0x%08lx",
                //  (const char*) driveName, aspiAddr);

                pListView->InsertItem(itemIndex, driveName);
                pListView->SetItemText(itemIndex, 1, remark);
                pListView->SetItemData(itemIndex, aspiAddr);
                itemIndex++;
            }
        }

        delete[] deviceArray;
#endif
    }

    *pItemIndex = itemIndex;
    return true;
}

bool OpenVolumeDialog::HasPhysicalDriveWin9x(int unit, CString* pRemark)
{
    HANDLE handle = NULL;
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
        return false;   // floppy drives only

    handle = CreateFile(L"\\\\.\\vwin32", 0, 0, NULL,
                OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        LOGI(" Unable to open vwin32: %ld", ::GetLastError());
        return false;
    }

#if 0   // didn't do what I wanted
    reg.reg_EAX = MAKEWORD(0, 0x00);    // func 0x00 == reset controller
    reg.reg_EDX = MAKEWORD(unit, 0);    // specify driver
    result = DeviceIoControl(handle, VWIN32_DIOC_DOS_INT13, &reg,
                sizeof(reg), &reg, sizeof(reg), &cb, 0);
    LOGI("  DriveReset(drive=0x%02x) result=%d carry=%d",
        unit, result, reg.reg_Flags & CARRY_FLAG);
#endif

    reg.reg_EAX = MAKEWORD(1, 0x02);    // read 1 sector
    reg.reg_EBX = (DWORD) buf;
    reg.reg_ECX = MAKEWORD(1, 0);       // sector 0 (+1), cylinder 0
    reg.reg_EDX = MAKEWORD(unit, 0);    // head

    result = DeviceIoControl(handle, VWIN32_DIOC_DOS_INT13, &reg,
                sizeof(reg) /*bytes*/, &reg, sizeof(reg) /*bytes*/, &cb, 0);
    lastError = GetLastError();
    ::CloseHandle(handle);

    if (result == 0 || (reg.reg_Flags & CARRY_FLAG)) {
        int ah = HIBYTE(reg.reg_EAX);
        LOGI(" DevIoCtrl(unit=%02xh) failed: result=%d lastErr=%d Flags=0x%08lx",
            unit, result, lastError, reg.reg_Flags);
        LOGI("   AH=%d (EAX=0x%08lx) byte=0x%02x", ah, reg.reg_EAX, buf[0]);
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

bool OpenVolumeDialog::HasPhysicalDriveWin2K(int unit, CString* pRemark)
{
    HANDLE hDevice;         // handle to the drive to be examined 
    DISK_GEOMETRY dg;       // disk drive geometry structure
    DISK_GEOMETRY_EX dge;   // extended geometry request buffer
    BOOL result;            // results flag
    DWORD junk;             // discard results
    LONGLONG diskSize;      // size of the drive, in bytes
    CString fileName;
    DWORD err;

    /*
     * See if the drive is there.
     */
    ASSERT(unit >= 128 && unit < 160);      // arbitrary max
    fileName.Format(L"\\\\.\\PhysicalDrive%d", unit - 128);

    hDevice = ::CreateFile(fileName,  // drive to open
                0,                  // no access to the drive
                FILE_SHARE_READ | FILE_SHARE_WRITE, // share mode
                NULL,               // default security attributes
                OPEN_EXISTING,      // disposition
                0,                  // file attributes
                NULL);              // do not copy file attributes

    if (hDevice == INVALID_HANDLE_VALUE) // cannot open the drive
        return false;

    /*
     * Try to get the drive geometry.  First try with the fancy WinXP call,
     * then fall back to the Win2K call if it doesn't exist.
     */
    result = ::DeviceIoControl(hDevice,
                IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                NULL, 0,                // input buffer
                &dge, sizeof(dge),      // output buffer + size in bytes
                &junk,                  // # bytes returned
                (LPOVERLAPPED) NULL);   // synchronous I/O
    if (result) {
        diskSize = dge.DiskSize.QuadPart;
        LOGI(" EX results for device %02xh", unit);
        LOGI("  Disk size = %I64d (bytes) = %I64d (MB)",
            diskSize, diskSize / (1024*1024));
        if (diskSize > 1024*1024*1024)
            pRemark->Format(L"Size is %.2fGB",
                (double) diskSize / (1024.0 * 1024.0 * 1024.0));
        else
            pRemark->Format(L"Size is %.2fMB",
                (double) diskSize / (1024.0 * 1024.0));
    } else {
        // Win2K shows ERROR_INVALID_FUNCTION or ERROR_NOT_SUPPORTED
        LOGI("IOCTL_DISK_GET_DRIVE_GEOMETRY_EX failed, error was %ld",
            GetLastError());
        result = ::DeviceIoControl(hDevice, // device to be queried
                    IOCTL_DISK_GET_DRIVE_GEOMETRY,  // operation to perform
                    NULL, 0,                // no input buffer
                    &dg, sizeof(dg),        // output buffer + size in bytes
                    &junk,                  // # bytes returned
                    (LPOVERLAPPED) NULL);   // synchronous I/O

        if (result) {
            LOGI(" Results for device %02xh", unit);
            LOGI("  Cylinders = %I64d", dg.Cylinders.QuadPart);
            LOGI("  Tracks per cylinder = %ld", (ULONG) dg.TracksPerCylinder);
            LOGI("  Sectors per track = %ld", (ULONG) dg.SectorsPerTrack);
            LOGI("  Bytes per sector = %ld", (ULONG) dg.BytesPerSector);

            diskSize = dg.Cylinders.QuadPart * (ULONG)dg.TracksPerCylinder *
                        (ULONG)dg.SectorsPerTrack * (ULONG)dg.BytesPerSector;
            LOGI("Disk size = %I64d (bytes) = %I64d (MB)", diskSize,
                    diskSize / (1024 * 1024));
            if (diskSize > 1024*1024*1024)
                pRemark->Format(L"Size is %.2fGB",
                    (double) diskSize / (1024.0 * 1024.0 * 1024.0));
            else
                pRemark->Format(L"Size is %.2fMB",
                    (double) diskSize / (1024.0 * 1024.0));
        } else {
            err = GetLastError();
        }
    }

    ::CloseHandle(hDevice);

    if (!result) {
        LOGI("DeviceIoControl(IOCTL_DISK_GET_DRIVE_GEOMETRY) failed (err=%ld)",
            err);
        *pRemark = "Not ready";
    }

    return true;
}

void OpenVolumeDialog::OnListChange(NMHDR*, LRESULT* pResult)
{
    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_VOLUME_LIST);
    CButton* pButton = (CButton*) GetDlgItem(IDOK);
    pButton->EnableWindow(pListView->GetSelectedCount() != 0);
    //LOGI("ENABLE %d", pListView->GetSelectedCount() != 0);

    *pResult = 0;
}

void OpenVolumeDialog::OnListDblClick(NMHDR* pNotifyStruct, LRESULT* pResult)
{
    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_VOLUME_LIST);
    CButton* pButton = (CButton*) GetDlgItem(IDOK);

    if (pListView->GetSelectedCount() != 0) {
        pButton->EnableWindow();
        OnOK();
    }

    *pResult = 0;
}

void OpenVolumeDialog::OnVolumeFilterSelChange(void)
{
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_VOLUME_FILTER);
    ASSERT(pCombo != NULL);
    LOGI("+++ SELECTION IS NOW %d", pCombo->GetCurSel());
    LoadDriveList();
}

void OpenVolumeDialog::OnOK(void)
{
    /*
     * Figure out the (zero-based) drive letter.
     */
    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_VOLUME_LIST);
    ASSERT(pListView != NULL);

    if (pListView->GetSelectedCount() != 1) {
        CString msg, failed;
        CheckedLoadString(&failed, IDS_FAILED);
        CheckedLoadString(&msg, IDS_VOLUME_SELECT_ONE);
        MessageBox(msg, failed, MB_OK);
        return;
    }

    POSITION posn;
    posn = pListView->GetFirstSelectedItemPosition();
    if (posn == NULL) {
        ASSERT(false);
        return;
    }
    int num = pListView->GetNextSelectedItem(posn);
    DWORD driveID = pListView->GetItemData(num);
    UINT formatID = 0;

    if (HIBYTE(HIWORD(driveID)) == 0xaa) {
        // TODO: remove this?
        fChosenDrive.Format(L"%hs%d:%d:%d\\",
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
            break;              // allow
        case DRIVE_CDROM:       //formatID = IDS_VOLUME_NO_CDROM;
            ForceReadOnly(true);
            break;
        case DRIVE_REMOTE:      formatID = IDS_VOLUME_NO_REMOTE;
            break;
        case DRIVE_RAMDISK:     formatID = IDS_VOLUME_NO_RAMDISK;
            break;
        case DRIVE_UNKNOWN:
        case DRIVE_NO_ROOT_DIR:
        default:                formatID = IDS_VOLUME_NO_GENERIC;
            break;
        }

        fChosenDrive.Format(L"%c:\\", driveID);
    } else if ((driveID >= 0 && driveID < 4) ||
               (driveID >= 0x80 && driveID < 0x88))
    {
        fChosenDrive.Format(L"%02x:\\", driveID);
    } else {
        ASSERT(false);
        return;
    }

    if (formatID != 0) {
        CString msg, notAllowed;

        CheckedLoadString(&notAllowed, IDS_NOT_ALLOWED);
        CheckedLoadString(&msg, formatID);
        MessageBox(msg, notAllowed, MB_OK);
    } else {
        Preferences* pPreferences = GET_PREFERENCES_WR();
        CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_VOLUME_FILTER);
        pPreferences->SetPrefLong(kPrVolumeFilter, pCombo->GetCurSel());
        LOGI("SETTING PREF TO %ld", pCombo->GetCurSel());

        CDialog::OnOK();
    }
}

void OpenVolumeDialog::ForceReadOnly(bool readOnly) const
{
    CButton* pButton = (CButton*) GetDlgItem(IDC_OPENVOL_READONLY);
    ASSERT(pButton != NULL);

    if (readOnly)
        pButton->SetCheck(BST_CHECKED);
    else
        pButton->SetCheck(BST_UNCHECKED);
    LOGW("FORCED READ ONLY %d", readOnly);
}
