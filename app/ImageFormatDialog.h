/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#ifndef APP_IMAGEFORMATDIALOG_H
#define APP_IMAGEFORMATDIALOG_H

#include "resource.h"
#include "../diskimg/DiskImg.h"
using namespace DiskImgLib;

/*
 * Dialog asking the user to confirm certain details of a disk image.
 *
 * The default values can be initialized individually or from a prepped
 * DiskImg structure.
 */
class ImageFormatDialog : public CDialog {
public:
    ImageFormatDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_DECONF, pParentWnd)
    {
        fInitialized = false;
        fFileSource = L"";
        fAllowUnknown = false;
        fOuterFormat = DiskImg::kOuterFormatUnknown;
        fFileFormat = DiskImg::kFileFormatUnknown;
        fPhysicalFormat = DiskImg::kPhysicalFormatUnknown;
        fSectorOrder = DiskImg::kSectorOrderUnknown;
        fFSFormat = DiskImg::kFormatUnknown;
        fDisplayFormat = kShowAsBlocks;

        fQueryDisplayFormat = true;
        fAllowGenericFormats = true;
        fHasSectors = fHasBlocks = fHasNibbles = false;
    }

    /*
     * Initialize our members by querying the associated DiskImg.
     */
    void InitializeValues(const DiskImg* pImg);

    bool                    fInitialized;
    CString                 fFileSource;
    bool                    fAllowUnknown;  // allow "unknown" choice?

    DiskImg::OuterFormat    fOuterFormat;
    DiskImg::FileFormat     fFileFormat;
    DiskImg::PhysicalFormat fPhysicalFormat;
    DiskImg::SectorOrder    fSectorOrder;
    DiskImg::FSFormat       fFSFormat;

    enum { kShowAsBlocks=0, kShowAsSectors=1, kShowAsNibbles=2 };
    int                     fDisplayFormat;

    void SetQueryDisplayFormat(bool val) { fQueryDisplayFormat = val; }
    void SetAllowGenericFormats(bool val) { fAllowGenericFormats = val; }

protected:
    virtual BOOL OnInitDialog(void) override;

    /*
     * Handle the "OK" button by extracting values from the dialog and
     * verifying that reasonable settings are in place.
     */
    void OnOK(void) override;

    afx_msg void OnHelp(void) {
        MyApp::HandleHelp(this, HELP_TOPIC_DISK_IMAGES);
    }
    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo) {
        return MyApp::HandleHelpInfo(lpHelpInfo);
    }

    struct ConvTable;

    /*
     * Load the combo boxes with every possible entry, and set the current
     * value appropriately.
     *
     * While we're at it, initialize the "source" edit text box and the
     * "show as blocks" checkbox.
     */
    void LoadComboBoxes(void);

    /*
     * Load the strings from ConvTable into the combo box, setting the
     * entry matching "default" as the current entry.
     */
    void LoadComboBox(int boxID, const ConvTable* pTable, int dflt);

    /*
     * Find the enum value for the specified index.
     */
    int ConvComboSel(int boxID, const ConvTable* pTable);

    bool                    fQueryDisplayFormat;
    bool                    fAllowGenericFormats;
    bool                    fHasSectors;
    bool                    fHasBlocks;
    bool                    fHasNibbles;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_IMAGEFORMATDIALOG_H*/
