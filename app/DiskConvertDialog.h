/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Let the user choose how they want to convert a disk image.
 */
#ifndef APP_DISKCONVERTDIALOG_H
#define APP_DISKCONVERTDIALOG_H

#include "resource.h"
#include "../diskimg/DiskImg.h"
#include "HelpTopics.h"

/*
 * The set of conversions available depends on the format of the source image.
 */
class DiskConvertDialog : public CDialog {
public:
    DiskConvertDialog(CWnd* pParentWnd) : CDialog(IDD_DISKCONV, pParentWnd)
    {
        fAllowUnadornedDOS = fAllowUnadornedProDOS = fAllowProDOS2MG =
            fAllowUnadornedNibble = fAllowD13 = fAllowDiskCopy42 =
            fAllowNuFX = fAllowTrackStar = fAllowSim2eHDV = fAllowDDD = false;
        fAddGzip = FALSE;
        fConvertIdx = -1;
        fBulkFileCount = -1;
        fSizeInBlocks = -1;
    }
    virtual ~DiskConvertDialog(void) {}

    /*
     * Initialize the set of available options based on the source image.
     */
    void Init(const DiskImgLib::DiskImg* pDiskImg);

    /*
     * Initialize options for a bulk transfer.
     */
    void Init(int fileCount);

    /* must match up with dialog */
    enum {
        kConvDOSRaw = 0,
        kConvDOS2MG = 1,
        kConvProDOSRaw = 2,
        kConvProDOS2MG = 3,
        kConvNibbleRaw = 4,
        kConvNibble2MG = 5,
        kConvD13 = 6,
        kConvDiskCopy42 = 7,
        kConvNuFX = 8,
        kConvTrackStar = 9,
        kConvSim2eHDV = 10,
        kConvDDD = 11,
    };
    int     fConvertIdx;

    BOOL    fAddGzip;

    // this is set to proper extension for the type chosen (e.g. "do")
    CString fExtension;

private:
    BOOL OnInitDialog(void) override;
    void DoDataExchange(CDataExchange* pDX) override;

    /*
     * If the radio button selection changes, we may need to disable the gzip
     * checkbox to show that NuFX can't be combined with gzip.
     *
     * If the underlying disk is over 32MB, disable gzip, because we won't be
     * able to open the disk we create.
     */
    afx_msg void OnChangeRadio(UINT nID);

    // User pressed the "Help" button.
    afx_msg void OnHelp(void) {
        if (fBulkFileCount < 0)
            MyApp::HandleHelp(this, HELP_TOPIC_DISK_CONV);
        else
            MyApp::HandleHelp(this, HELP_TOPIC_BULK_DISK_CONV);
    }

    // Context help request (question mark button).
    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo) {
        return MyApp::HandleHelpInfo(lpHelpInfo);
    }

    CString fDiskDescription;
    bool    fAllowUnadornedDOS;
    bool    fAllowUnadornedProDOS;
    bool    fAllowProDOS2MG;
    bool    fAllowUnadornedNibble;
    bool    fAllowD13;
    bool    fAllowDiskCopy42;
    bool    fAllowNuFX;
    bool    fAllowTrackStar;
    bool    fAllowSim2eHDV;
    bool    fAllowDDD;

    int     fBulkFileCount;

    long    fSizeInBlocks;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_DISKCONVERTDIALOG_H*/
