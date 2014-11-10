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
    virtual BOOL OnInitDialog(void);
    virtual void DoDataExchange(CDataExchange* pDX);

//  BOOL OnHelpInfo(HELPINFO* lpHelpInfo);

    afx_msg void OnFormatChangeRange(UINT nID);
    afx_msg void OnSizeChangeRange(UINT nID);
    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo);
    afx_msg void OnHelp(void);

    bool IsValidVolumeName_DOS(const WCHAR* name);
    bool IsValidVolumeName_ProDOS(const WCHAR* name);
    bool IsValidVolumeName_Pascal(const WCHAR* name);
    bool IsValidVolumeName_HFS(const WCHAR* name);

    bool    fExtendedOpts;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_CREATEIMAGEDIALOG_H*/
