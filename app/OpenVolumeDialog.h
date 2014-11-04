/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Class definition for Open Volume dialog.
 */
#ifndef __OPEN_VOLUME_DIALOG__
#define __OPEN_VOLUME_DIALOG__

#include <afxwin.h>
#include "resource.h"

/*
 * A dialog with a list control that we populate with the names of the
 * volumes in the system.
 */
class OpenVolumeDialog : public CDialog {
public:
    OpenVolumeDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_OPENVOLUMEDLG, pParentWnd),
        fChosenDrive(""),
        fAllowROChange(true)
    {
        Preferences* pPreferences = GET_PREFERENCES_WR();
        fReadOnly = pPreferences->GetPrefBool(kPrOpenVolumeRO);
    }

    // Result: drive to open (e.g. "A:\" or "80:\")
    CString fChosenDrive;

    // Did the user check "read only"? (sets default and holds return val)
    BOOL fReadOnly;
    // Set before calling DoModal to disable "read only" checkbox
    bool fAllowROChange;

protected:
    virtual BOOL OnInitDialog(void);
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual void OnOK(void);
    
    afx_msg void OnHelp(void);
    afx_msg void OnListChange(NMHDR* pNotifyStruct, LRESULT* pResult);
    afx_msg void OnListDblClick(NMHDR* pNotifyStruct, LRESULT* pResult);
    afx_msg void OnVolumeFilterSelChange(void);

    // 0 is default; numbers must match up with pop-up menu order
    // order also matters for range test in OnInitDialog
    enum { kBoth=0, kLogical=1, kPhysical=2 };
    // common constants
    enum { kMaxLogicalDrives = 26, kMaxPhysicalDrives = 8 };

    void LoadDriveList(void);
    bool LoadLogicalDriveList(CListCtrl* pListView, int* pItemIndex);
    bool LoadPhysicalDriveList(CListCtrl* pListView, int* pItemIndex);
    bool HasPhysicalDriveWin9x(int unit, CString* pRemark);
    bool HasPhysicalDriveWin2K(int unit, CString* pRemark);

    void ForceReadOnly(bool readOnly) const;

    struct {
        unsigned int driveType;
    } fVolumeInfo[kMaxLogicalDrives];


    DECLARE_MESSAGE_MAP()
};

#endif /*__OPEN_VOLUME_DIALOG__*/