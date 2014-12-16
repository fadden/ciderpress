/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for disk image conversion dialog
 */
#include "StdAfx.h"
#include "DiskConvertDialog.h"

using namespace DiskImgLib;

BEGIN_MESSAGE_MAP(DiskConvertDialog, CDialog)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_DISKCONV_DOS, IDC_DISKCONV_DDD, OnChangeRadio)
    ON_WM_HELPINFO()
    ON_COMMAND(IDHELP, OnHelp)
END_MESSAGE_MAP()


void DiskConvertDialog::Init(const DiskImg* pDiskImg)
{
    ASSERT(pDiskImg != NULL);
    const int kMagicNibbles = -1234;
    bool hasBlocks = pDiskImg->GetHasBlocks();
    bool hasSectors = pDiskImg->GetHasSectors();
    bool hasNibbles = pDiskImg->GetHasNibbles();
    long diskSizeInSectors;

    ASSERT(fSizeInBlocks == -1);
    if (hasBlocks) {
        diskSizeInSectors = pDiskImg->GetNumBlocks() * 2;
        fSizeInBlocks = diskSizeInSectors / 2;
    } else if (hasSectors) {
        diskSizeInSectors =
            pDiskImg->GetNumTracks() * pDiskImg->GetNumSectPerTrack();
        if (pDiskImg->GetNumSectPerTrack() != 13)
            fSizeInBlocks = diskSizeInSectors / 2;
    } else {
        ASSERT(hasNibbles);
        diskSizeInSectors = kMagicNibbles;
    }

    if (diskSizeInSectors >= 8388608) {
        /* no conversions for files 2GB and larger except .PO */
        fDiskDescription.Format(IDS_CDESC_BLOCKS, diskSizeInSectors/4);
        fAllowUnadornedProDOS = true;
        fConvertIdx = kConvProDOSRaw;
    } else if (diskSizeInSectors == 35*16) {
        /* 140K disk image */
        CheckedLoadString(&fDiskDescription, IDS_CDESC_140K);
        fAllowUnadornedDOS = true;
        fAllowUnadornedProDOS = true;
        fAllowProDOS2MG = true;
        fAllowUnadornedNibble = true;
        fAllowNuFX = true;
        fAllowTrackStar = true;
        fAllowSim2eHDV = true;
        fAllowDDD = true;
        if (hasNibbles)
            fConvertIdx = kConvNibbleRaw;
        else
            fConvertIdx = kConvDOSRaw;
    } else if (diskSizeInSectors == 40*16 &&
        (pDiskImg->GetFileFormat() == DiskImg::kFileFormatTrackStar ||
        pDiskImg->GetFileFormat() == DiskImg::kFileFormatFDI))
    {
        /* 40-track TrackStar or FDI image; allow conversion to 35-track formats */
        CheckedLoadString(&fDiskDescription, IDS_CDESC_40TRACK);
        ASSERT(pDiskImg->GetHasBlocks());
        fAllowUnadornedDOS = true;
        fAllowUnadornedProDOS = true;
        fAllowProDOS2MG = true;
        fAllowUnadornedNibble = true;
        fAllowNuFX = true;
        fAllowSim2eHDV = true;
        fAllowDDD = true;
        fAllowTrackStar = true;
        fConvertIdx = kConvDOSRaw;
    } else if (diskSizeInSectors == 35*13) {
        /* 13-sector 5.25" floppy */
        CheckedLoadString(&fDiskDescription, IDS_CDEC_140K_13);
        fAllowUnadornedNibble = true;
        fAllowD13 = true;
        fConvertIdx = kConvNibbleRaw;
    } else if (diskSizeInSectors == kMagicNibbles) {
        /* blob of nibbles with no recognizable format; allow self-convert */
        CheckedLoadString(&fDiskDescription, IDS_CDEC_RAWNIB);
        if (pDiskImg->GetPhysicalFormat() == DiskImg::kPhysicalFormatNib525_6656)
        {
            fAllowUnadornedNibble = true;
            fConvertIdx = kConvNibbleRaw;
        } else if (pDiskImg->GetPhysicalFormat() == DiskImg::kPhysicalFormatNib525_Var)
        {
            fAllowTrackStar = true;
            fConvertIdx = kConvTrackStar;
        } else if (pDiskImg->GetPhysicalFormat() == DiskImg::kPhysicalFormatNib525_6384)
        {
            /* don't currently allow .nb2 output */
            LOGI(" GLITCH: we don't allow self-convert of .nb2 files");
            ASSERT(false);
        } else {
            /* this should be impossible */
            ASSERT(false);
        }
    } else if (diskSizeInSectors == 3200) {
        /* 800K disk image */
        CheckedLoadString(&fDiskDescription, IDS_CDESC_800K);
        fAllowUnadornedDOS = true;
        fAllowUnadornedProDOS = true;
        fAllowProDOS2MG = true;
        fAllowDiskCopy42 = true;
        fAllowNuFX = true;
        fAllowSim2eHDV = true;
        fConvertIdx = kConvProDOS2MG;
    } else {
        /* odd-sized disk image; could allow DOS if hasSectors */
        fDiskDescription.Format(IDS_CDESC_BLOCKS, diskSizeInSectors/4);
        fAllowUnadornedProDOS = true;
        fAllowProDOS2MG = true;
        fAllowNuFX = true;
        fAllowSim2eHDV = true;
        fConvertIdx = kConvProDOS2MG;
    }
}

void DiskConvertDialog::Init(int fileCount)
{
    /* allow everything */
    fAllowUnadornedDOS = fAllowUnadornedProDOS = fAllowProDOS2MG =
        fAllowUnadornedNibble = fAllowD13 = fAllowDiskCopy42 =
        fAllowNuFX = fAllowTrackStar = fAllowSim2eHDV = fAllowDDD = true;
    fConvertIdx = kConvDOSRaw;      // default choice == first in list
    fBulkFileCount = fileCount;
    fDiskDescription.Format(L"%d images selected", fBulkFileCount);
}

BOOL DiskConvertDialog::OnInitDialog(void)
{
    CWnd* pWnd;

    ASSERT(fConvertIdx != -1);      // must call Init before DoModal

    if (!fAllowUnadornedDOS) {
        pWnd = GetDlgItem(IDC_DISKCONV_DOS);
        pWnd->EnableWindow(FALSE);
        pWnd = GetDlgItem(IDC_DISKCONV_DOS2MG);
        pWnd->EnableWindow(FALSE);
    }
    if (!fAllowUnadornedProDOS) {
        pWnd = GetDlgItem(IDC_DISKCONV_PRODOS);
        pWnd->EnableWindow(FALSE);
    }
    if (!fAllowProDOS2MG) {
        pWnd = GetDlgItem(IDC_DISKCONV_PRODOS2MG);
        pWnd->EnableWindow(FALSE);
    }
    if (!fAllowUnadornedNibble) {
        pWnd = GetDlgItem(IDC_DISKCONV_NIB);
        pWnd->EnableWindow(FALSE);
        pWnd = GetDlgItem(IDC_DISKCONV_NIB2MG);
        pWnd->EnableWindow(FALSE);
    }
    if (!fAllowD13) {
        pWnd = GetDlgItem(IDC_DISKCONV_D13);
        pWnd->EnableWindow(FALSE);
    }
    if (!fAllowDiskCopy42) {
        pWnd = GetDlgItem(IDC_DISKCONV_DC42);
        pWnd->EnableWindow(FALSE);
    }
    if (!fAllowNuFX) {
        pWnd = GetDlgItem(IDC_DISKCONV_SDK);
        pWnd->EnableWindow(FALSE);
    }
    if (!fAllowTrackStar) {
        pWnd = GetDlgItem(IDC_DISKCONV_TRACKSTAR);
        pWnd->EnableWindow(FALSE);
    }
    if (!fAllowSim2eHDV) {
        pWnd = GetDlgItem(IDC_DISKCONV_HDV);
        pWnd->EnableWindow(FALSE);
    }
    if (!fAllowDDD) {
        pWnd = GetDlgItem(IDC_DISKCONV_DDD);
        pWnd->EnableWindow(FALSE);
    }

    if (fBulkFileCount < 0) {
        pWnd = GetDlgItem(IDC_IMAGE_TYPE);
        pWnd->SetWindowText(fDiskDescription);
    } else {
        CRect rect;
        int right;
        pWnd = GetDlgItem(IDC_IMAGE_TYPE);
        pWnd->GetWindowRect(&rect);
        ScreenToClient(&rect);
        right = rect.right;
        pWnd->DestroyWindow();

        pWnd = GetDlgItem(IDC_IMAGE_SIZE_TEXT);
        pWnd->GetWindowRect(&rect);
        ScreenToClient(&rect);
        rect.right = right;
        pWnd->MoveWindow(&rect);
        pWnd->SetWindowText(fDiskDescription);
    }

    OnChangeRadio(0);       // set the gzip box

    CDialog::OnInitDialog();

    return TRUE;
}

void DiskConvertDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Check(pDX, IDC_DISKCONV_GZIP, fAddGzip);
    DDX_Radio(pDX, IDC_DISKCONV_DOS, fConvertIdx);

    if (pDX->m_bSaveAndValidate) {
        switch (fConvertIdx) {
        case kConvDOSRaw:       fExtension = L"do";     break;
        case kConvDOS2MG:       fExtension = L"2mg";    break;
        case kConvProDOSRaw:    fExtension = L"po";     break;
        case kConvProDOS2MG:    fExtension = L"2mg";    break;
        case kConvNibbleRaw:    fExtension = L"nib";    break;
        case kConvNibble2MG:    fExtension = L"2mg";    break;
        case kConvD13:          fExtension = L"d13";    break;
        case kConvDiskCopy42:   fExtension = L"dsk";    break;
        case kConvNuFX:         fExtension = L"sdk";    break;
        case kConvTrackStar:    fExtension = L"app";    break;
        case kConvSim2eHDV:     fExtension = L"hdv";    break;
        case kConvDDD:          fExtension = L"ddd";    break;
        default:
            fExtension = L"???";
            ASSERT(false);
            break;
        }

        if (fAddGzip && fConvertIdx != kConvNuFX) {
            fExtension += L".gz";
        }

        LOGI(" DCD recommending extension '%ls'", (LPCWSTR) fExtension);
    }
}

void DiskConvertDialog::OnChangeRadio(UINT nID)
{
    CWnd* pGzip = GetDlgItem(IDC_DISKCONV_GZIP);
    ASSERT(pGzip != NULL);
    CButton* pNuFX = (CButton*)GetDlgItem(IDC_DISKCONV_SDK);
    ASSERT(pNuFX != NULL);

    if (fSizeInBlocks  > DiskImgLib::kGzipMax / 512)
        pGzip->EnableWindow(FALSE);
    else
        pGzip->EnableWindow(pNuFX->GetCheck() == BST_UNCHECKED);
}
