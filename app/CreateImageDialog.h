/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Options for creating a blank disk image.
 */
#ifndef APP_CREATEIMAGEDIALOG_H
#define APP_CREATEIMAGEDIALOG_H

#include "resource.h"

/*
 * Get some options.
 */
class CreateImageDialog : public CDialog {
public:
    /* this must match up with control IDs in dialog */
    enum {
        kFmtDOS32 = 0,
        kFmtDOS33,
        kFmtProDOS,
        kFmtPascal,
        kFmtHFS,
        kFmtBlank
    };

    CreateImageDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_CREATEIMAGE, pParentWnd)
    {
        fDiskSizeIdx = 0;
        fDiskFormatIdx = kFmtProDOS;
        fAllocTracks_DOS = TRUE;
        fDOSVolumeNum = 254;
        fVolName_ProDOS = L"NEW.DISK";
        fVolName_Pascal = L"BLANK";
        fVolName_HFS = L"New Disk";
        fNumBlocks = -2;        // -1 has special meaning
        fExtendedOpts = false;
    }
    virtual ~CreateImageDialog(void) {}

    int     fDiskSizeIdx;
    int     fDiskFormatIdx;
    BOOL    fAllocTracks_DOS;
    int     fDOSVolumeNum;
    CString fVolName_ProDOS;
    CString fVolName_Pascal;
    CString fVolName_HFS;

    long    fNumBlocks;     // computed when DoModal finishes

private:
    virtual BOOL OnInitDialog(void) override;
    virtual void DoDataExchange(CDataExchange* pDX) override;

//  afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo);

    /*
     * When the user chooses a format, enable and disable controls as
     * appropriate.
     */
    afx_msg void OnFormatChangeRange(UINT nID);

    /*
     * When one of the radio buttons is clicked on, update the active status
     * and contents of the "specify size" edit box.
     */
    afx_msg void OnSizeChangeRange(UINT nID);

    // Context help (question mark).
    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo) {
        return MyApp::HandleHelpInfo(lpHelpInfo);
    }

    // Dialog help ("help" button).
    afx_msg void OnHelp(void) {
        MyApp::HandleHelp(this, HELP_TOPIC_IMAGE_CREATOR);
    }

    bool IsValidVolumeName_DOS(const WCHAR* name);
    bool IsValidVolumeName_ProDOS(const WCHAR* name);
    bool IsValidVolumeName_Pascal(const WCHAR* name);
    bool IsValidVolumeName_HFS(const WCHAR* name);

    bool    fExtendedOpts;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_CREATEIMAGEDIALOG_H*/
