/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#ifndef APP_OPENVOLUMEDIALOG_H
#define APP_OPENVOLUMEDIALOG_H

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
        fChosenDrive(L""),
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
    virtual BOOL OnInitDialog(void) override;
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK(void) override;
    
    /*
     * Something changed in the list.  Update the "OK" button.
     */
    afx_msg void OnListChange(NMHDR* pNotifyStruct, LRESULT* pResult);

    /*
     * The volume filter drop-down box has changed.
     */
    afx_msg void OnVolumeFilterSelChange(void);

    afx_msg void OnHelp(void) {
        MyApp::HandleHelp(this, HELP_TOPIC_OPEN_VOLUME);
    }

    afx_msg void OnListDblClick(NMHDR* pNotifyStruct, LRESULT* pResult);

    // 0 is default; numbers must match up with pop-up menu order
    // order also matters for range test in OnInitDialog
    enum { kBoth=0, kLogical=1, kPhysical=2 };
    // common constants
    enum { kMaxLogicalDrives = 26, kMaxPhysicalDrives = 8 };

    /*
     * Load the set of logical and physical drives.
     */
    void LoadDriveList(void);

    /*
     * Determine the logical volumes available in the system and stuff them into
     * the list.
     */
    bool LoadLogicalDriveList(CListCtrl* pListView, int* pItemIndex);

    /*
     * Add a list of physical drives to the list control.
     */
    bool LoadPhysicalDriveList(CListCtrl* pListView, int* pItemIndex);

    /*
     * Determine whether physical device N exists.
     *
     * Pass in the Int13 unit number, i.e. 0x00 for the first floppy drive.  Win9x
     * makes direct access to the hard drive very difficult, so we don't even try.
     *
     * TODO: remove this entirely?
     */
    bool HasPhysicalDriveWin9x(int unit, CString* pRemark);

    /*
     * Determine whether physical device N exists.
     *
     * Pass in the Int13 unit number, i.e. 0x80 for the first hard drive.  This
     * should not be called with units for floppy drives (e.g. 0x00).
     */
    bool HasPhysicalDriveWin2K(int unit, CString* pRemark);

    /*
     * Set the state of the "read only" checkbox in the dialog.
     */
    void ForceReadOnly(bool readOnly) const;

    struct {
        unsigned int driveType;
    } fVolumeInfo[kMaxLogicalDrives];


    DECLARE_MESSAGE_MAP()
};

#endif /*APP_OPENVOLUMEDIALOG_H*/
