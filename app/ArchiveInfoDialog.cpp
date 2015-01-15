/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of the various ArchiveInfo dialog classes.
 */
#include "StdAfx.h"
#include "ArchiveInfoDialog.h"
#include "../nufxlib/NufxLib.h"
#include "../reformat/Charset.h"

/*
 * ===========================================================================
 *      ArchiveInfoDialog
 * ===========================================================================
 */

BEGIN_MESSAGE_MAP(ArchiveInfoDialog, CDialog)
    ON_COMMAND(IDHELP, OnHelp)
END_MESSAGE_MAP()


/*
 * ===========================================================================
 *      NufxArchiveInfoDialog
 * ===========================================================================
 */

BOOL NufxArchiveInfoDialog::OnInitDialog(void)
{
    CString notAvailable = "(not available)";
    NuArchive* pNuArchive;
    const NuMasterHeader* pMasterHeader;
    CWnd* pWnd;
    CString tmpStr;
    NuAttr attr;
    NuError nerr;
    time_t when;

    ASSERT(fpArchive != NULL);

    pNuArchive = fpArchive->GetNuArchivePointer();
    ASSERT(pNuArchive != NULL);
    (void) NuGetMasterHeader(pNuArchive, &pMasterHeader);
    ASSERT(pMasterHeader != NULL);

    pWnd = GetDlgItem(IDC_AI_FILENAME);
    CString pathName(fpArchive->GetPathName());
    pWnd->SetWindowText(pathName);

    pWnd = GetDlgItem(IDC_AINUFX_RECORDS);
    nerr = NuGetAttr(pNuArchive, kNuAttrNumRecords, &attr);
    if (nerr == kNuErrNone)
        tmpStr.Format(L"%ld", attr);
    else
        tmpStr = notAvailable;
    pWnd->SetWindowText(tmpStr);

    pWnd = GetDlgItem(IDC_AINUFX_FORMAT);
    nerr = NuGetAttr(pNuArchive, kNuAttrArchiveType, &attr);
    switch (attr) {
    case kNuArchiveNuFX:            tmpStr = L"NuFX";                   break;
    case kNuArchiveNuFXInBNY:       tmpStr = L"NuFX in Binary II";      break;
    case kNuArchiveNuFXSelfEx:      tmpStr = L"Self-extracting NuFX";   break;
    case kNuArchiveNuFXSelfExInBNY: tmpStr = L"Self-extracting NuFX in Binary II";
        break;
    case kNuArchiveBNY:             tmpStr = L"Binary II";              break;
    default:
        tmpStr = L"(unknown)";
        break;
    };
    pWnd->SetWindowText(tmpStr);

    pWnd = GetDlgItem(IDC_AINUFX_MASTERVERSION);
    tmpStr.Format(L"%ld", pMasterHeader->mhMasterVersion);
    pWnd->SetWindowText(tmpStr);

    pWnd = GetDlgItem(IDC_AINUFX_CREATEWHEN);
    when = NufxArchive::DateTimeToSeconds(&pMasterHeader->mhArchiveCreateWhen);
    tmpStr.Format(L"%.24hs", ctime(&when));
    pWnd->SetWindowText(tmpStr);

    pWnd = GetDlgItem(IDC_AINUFX_MODIFYWHEN);
    when = NufxArchive::DateTimeToSeconds(&pMasterHeader->mhArchiveModWhen);
    tmpStr.Format(L"%.24hs", ctime(&when));
    pWnd->SetWindowText(tmpStr);

    pWnd = GetDlgItem(IDC_AINUFX_JUNKSKIPPED);
    nerr = NuGetAttr(pNuArchive, kNuAttrJunkOffset, &attr);
    if (nerr == kNuErrNone)
        tmpStr.Format(L"%ld bytes", attr);
    else
        tmpStr = notAvailable;
    pWnd->SetWindowText(tmpStr);

    return ArchiveInfoDialog::OnInitDialog();
}


/*
 * ===========================================================================
 *      DiskArchiveInfoDialog
 * ===========================================================================
 */

BEGIN_MESSAGE_MAP(DiskArchiveInfoDialog, ArchiveInfoDialog)
    ON_CBN_SELCHANGE(IDC_AIDISK_SUBVOLSEL, OnSubVolSelChange)
END_MESSAGE_MAP()

BOOL DiskArchiveInfoDialog::OnInitDialog(void)
{
    CWnd* pWnd;
    CString tmpStr;
    const DiskImg* pDiskImg;
    const DiskFS* pDiskFS;

    ASSERT(fpArchive != NULL);

    pDiskImg = fpArchive->GetDiskImg();
    ASSERT(pDiskImg != NULL);
    pDiskFS = fpArchive->GetDiskFS();
    ASSERT(pDiskFS != NULL);

    /*
     * Volume characteristics.
     */
    pWnd = GetDlgItem(IDC_AI_FILENAME);
    pWnd->SetWindowText(fpArchive->GetPathName());

    pWnd = GetDlgItem(IDC_AIDISK_OUTERFORMAT);
    CStringW outerFormat(DiskImg::ToString(pDiskImg->GetOuterFormat()));
    pWnd->SetWindowText(outerFormat);

    pWnd = GetDlgItem(IDC_AIDISK_FILEFORMAT);
    CStringW fileFormat(DiskImg::ToString(pDiskImg->GetFileFormat()));
    pWnd->SetWindowText(fileFormat);

    pWnd = GetDlgItem(IDC_AIDISK_PHYSICALFORMAT);
    DiskImg::PhysicalFormat physicalFormat = pDiskImg->GetPhysicalFormat();
    if (physicalFormat == DiskImg::kPhysicalFormatNib525_6656 ||
        physicalFormat == DiskImg::kPhysicalFormatNib525_6384 ||
        physicalFormat == DiskImg::kPhysicalFormatNib525_Var)
    {
        CString tmpStr;
        const DiskImg::NibbleDescr* pNibbleDescr = pDiskImg->GetNibbleDescr();
        if (pNibbleDescr != NULL)
            tmpStr.Format(L"%hs, layout is \"%hs\"",
                DiskImg::ToString(physicalFormat), pNibbleDescr->description);
        else
            tmpStr = DiskImg::ToString(physicalFormat); // unexpected
        pWnd->SetWindowText(tmpStr);
    } else {
        CString physicalFormat(DiskImg::ToString(physicalFormat));
        pWnd->SetWindowText(physicalFormat);
    }

    FillInVolumeInfo(pDiskFS);

    /*
     * Configure the sub-volume drop down menu.  If there's only one item,
     * we disable it.
     */
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_AIDISK_SUBVOLSEL);
    int idx = 0;

    AddSubVolumes(pDiskFS, L"", &idx);
    ASSERT(idx > 0);        // must have at least the top-level DiskFS

    pCombo->SetCurSel(0);
    if (idx == 1)
        pCombo->EnableWindow(FALSE);

    return ArchiveInfoDialog::OnInitDialog();
}

void DiskArchiveInfoDialog::AddSubVolumes(const DiskFS* pDiskFS,
    const WCHAR* prefix, int* pIdx)
{
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_AIDISK_SUBVOLSEL);

    /*
     * Add the current DiskFS.
     */
    CString tmpStr(prefix);
    tmpStr += Charset::ConvertMORToUNI(pDiskFS->GetVolumeID());
    pCombo->AddString(tmpStr);
    pCombo->SetItemData(*pIdx, (unsigned long) pDiskFS);
    (*pIdx)++;

    /*
     * Add everything beneath the current level.
     */
    DiskFS::SubVolume* pSubVol;
    pSubVol = pDiskFS->GetNextSubVolume(NULL);
    tmpStr = prefix;
    tmpStr += L"   ";
    while (pSubVol != NULL) {
        AddSubVolumes(pSubVol->GetDiskFS(), tmpStr, pIdx);

        pSubVol = pDiskFS->GetNextSubVolume(pSubVol);
    }
}

void DiskArchiveInfoDialog::OnSubVolSelChange(void)
{
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_AIDISK_SUBVOLSEL);
    ASSERT(pCombo != NULL);
    //LOGI("+++ SELECTION IS NOW %d", pCombo->GetCurSel());

    const DiskFS* pDiskFS;
    pDiskFS = (DiskFS*) pCombo->GetItemData(pCombo->GetCurSel());
    ASSERT(pDiskFS != NULL);
    FillInVolumeInfo(pDiskFS);
}

void DiskArchiveInfoDialog::FillInVolumeInfo(const DiskFS* pDiskFS)
{
    const DiskImg* pDiskImg = pDiskFS->GetDiskImg();
    CString unknown = L"(unknown)";
    CString tmpStr;
    DIError dierr;
    CWnd* pWnd;

    pWnd = GetDlgItem(IDC_AIDISK_SECTORORDER);
    CStringW sectorOrderW(DiskImg::ToString(pDiskImg->GetSectorOrder()));
    pWnd->SetWindowText(sectorOrderW);

    pWnd = GetDlgItem(IDC_AIDISK_FSFORMAT);
    CStringW fsFormat(DiskImg::ToString(pDiskImg->GetFSFormat()));
    pWnd->SetWindowText(fsFormat);

    pWnd = GetDlgItem(IDC_AIDISK_FILECOUNT);
    tmpStr.Format(L"%ld", pDiskFS->GetFileCount());
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
            tmpStr.Format(L"%ld blocks (%ls)",
                totalUnits, (LPCWSTR) reducedSize);
            if (totalUnits != pDiskImg->GetNumBlocks()) {
                CString tmpStr2;
                tmpStr2.Format(L", image has room for %ld blocks",
                    pDiskImg->GetNumBlocks());
                tmpStr += tmpStr2;
            }
            pWnd->SetWindowText(tmpStr);

            pWnd = GetDlgItem(IDC_AIDISK_FREESPACE);
            GetReducedSize(freeUnits, unitSize, &reducedSize);
            tmpStr.Format(L"%ld blocks (%ls)",
                freeUnits, (LPCWSTR) reducedSize);
            pWnd->SetWindowText(tmpStr);
        } else {
            ASSERT(unitSize == DiskImgLib::kSectorSize);

            pWnd = GetDlgItem(IDC_AIDISK_CAPACITY);
            GetReducedSize(totalUnits, unitSize, &reducedSize);
            tmpStr.Format(L"%ld sectors (%ls)",
                totalUnits, (LPCWSTR) reducedSize);
            pWnd->SetWindowText(tmpStr);

            pWnd = GetDlgItem(IDC_AIDISK_FREESPACE);
            GetReducedSize(freeUnits, unitSize, &reducedSize);
            tmpStr.Format(L"%ld sectors (%ls)",
                freeUnits, (LPCWSTR) reducedSize);
            pWnd->SetWindowText(tmpStr);
        }
    } else {
        /* "free space" not supported; fill in what we do know */
        pWnd = GetDlgItem(IDC_AIDISK_CAPACITY);
        if (pDiskImg->GetHasBlocks()) {
            totalUnits = pDiskImg->GetNumBlocks();
            GetReducedSize(totalUnits, DiskImgLib::kBlockSize, &reducedSize);
            tmpStr.Format(L"%ld blocks (%ls)",
                totalUnits, (LPCWSTR) reducedSize);
        } else if (pDiskImg->GetHasSectors()) {
            tmpStr.Format(L"%ld tracks, %d sectors per track",
                pDiskImg->GetNumTracks(), pDiskImg->GetNumSectPerTrack());
        } else {
            tmpStr = unknown;
        }
        pWnd->SetWindowText(tmpStr);

        pWnd = GetDlgItem(IDC_AIDISK_FREESPACE);
        pWnd->SetWindowText(unknown);
    }

    pWnd = GetDlgItem(IDC_AIDISK_WRITEABLE);
    tmpStr = pDiskFS->GetReadWriteSupported() ? L"Yes" : L"No";
    pWnd->SetWindowText(tmpStr);

    pWnd = GetDlgItem(IDC_AIDISK_DAMAGED);
    tmpStr = pDiskFS->GetFSDamaged() ? L"Yes" : L"No";
    pWnd->SetWindowText(tmpStr);

    const char* cp;
    WCHAR* outp;

    pWnd = GetDlgItem(IDC_AIDISK_NOTES);
    cp = pDiskImg->GetNotes();
    // GetBuffer wants length in code units, which will be 2x since it's
    // wide chars.  The 2x mult below is for worst-case linefeed conversion.
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
        tmpStr.TrimRight();     // trim the whitespace chars off
    pWnd->SetWindowText(tmpStr);
}

void DiskArchiveInfoDialog::GetReducedSize(long numUnits, int unitSize,
    CString* pOut) const
{
    LONGLONG sizeInBytes = numUnits;
    sizeInBytes *= unitSize;
    long reducedSize;

    if (sizeInBytes < 0) {
        ASSERT(false);
        pOut->Format(L"<bogus>");
        return;
    }

    if (sizeInBytes >= 1024*1024*1024) {
        reducedSize = (long) (sizeInBytes / (1024*1024));
        pOut->Format(L"%.2fGB", reducedSize / 1024.0);
    } else if (sizeInBytes >= 1024*1024) {
        reducedSize = (long) (sizeInBytes / 1024);
        pOut->Format(L"%.2fMB", reducedSize / 1024.0);
    } else {
        pOut->Format(L"%.2fKB", ((long) sizeInBytes) / 1024.0);
    }
}


/*
 * ===========================================================================
 *      BnyArchiveInfoDialog
 * ===========================================================================
 */

BOOL BnyArchiveInfoDialog::OnInitDialog(void)
{
    CWnd* pWnd;
    CString tmpStr;

    ASSERT(fpArchive != NULL);

    pWnd = GetDlgItem(IDC_AI_FILENAME);
    pWnd->SetWindowText(fpArchive->GetPathName());
    tmpStr.Format(L"%ld", fpArchive->GetNumEntries());
    pWnd = GetDlgItem(IDC_AIBNY_RECORDS);
    pWnd->SetWindowText(tmpStr);

    return ArchiveInfoDialog::OnInitDialog();
}


/*
 * ===========================================================================
 *      AcuArchiveInfoDialog
 * ===========================================================================
 */

BOOL AcuArchiveInfoDialog::OnInitDialog(void)
{
    CWnd* pWnd;
    CString tmpStr;

    ASSERT(fpArchive != NULL);

    pWnd = GetDlgItem(IDC_AI_FILENAME);
    pWnd->SetWindowText(fpArchive->GetPathName());
    tmpStr.Format(L"%ld", fpArchive->GetNumEntries());
    pWnd = GetDlgItem(IDC_AIBNY_RECORDS);
    pWnd->SetWindowText(tmpStr);

    return ArchiveInfoDialog::OnInitDialog();
}

/*
 * ===========================================================================
 *      AppleSingleArchiveInfoDialog
 * ===========================================================================
 */

BOOL AppleSingleArchiveInfoDialog::OnInitDialog(void)
{
    CWnd* pWnd;

    ASSERT(fpArchive != NULL);

    pWnd = GetDlgItem(IDC_AI_FILENAME);
    pWnd->SetWindowText(fpArchive->GetPathName());
    pWnd = GetDlgItem(IDC_AIBNY_RECORDS);
    pWnd->SetWindowText(fpArchive->GetInfoString());

    return ArchiveInfoDialog::OnInitDialog();
}
