/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Dialog that allows copying volumes or sub-volumes to and from files on
 * disk.  Handy for backing up and restoring floppy disks and CFFA partitions.
 */
#ifndef APP_VOLUMECOPYDIALOG_H
#define APP_VOLUMECOPYDIALOG_H

#include <afxwin.h>
#include "../diskimg/DiskImg.h"
#include "resource.h"

/*
 * A dialog with a list control that we populate with the names of the
 * volumes in the system.
 */
class VolumeCopyDialog : public CDialog {
public:
    VolumeCopyDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_VOLUMECOPYSEL, pParentWnd),
        fpDiskImg(nil),
        fpDiskFS(nil),
        fpWaitDlg(nil)
    {}
    ~VolumeCopyDialog(void) { assert(fpDiskFS == nil); }

    /* disk image to work with; we don't own it */
    DiskImgLib::DiskImg*    fpDiskImg;
    /* path name of input disk image or volume; mainly for display */
    CString     fPathName;

protected:
    virtual BOOL OnInitDialog(void);
    //virtual void DoDataExchange(CDataExchange* pDX);
    virtual void OnOK(void);
    virtual void OnCancel(void);

    void Cleanup(void);
    
    enum { WMU_DIALOG_READY = WM_USER+2 };

    afx_msg void OnHelp(void);
    afx_msg void OnListChange(NMHDR* pNotifyStruct, LRESULT* pResult);
    afx_msg void OnCopyToFile(void);
    afx_msg void OnCopyFromFile(void);

    afx_msg LONG OnDialogReady(UINT, LONG);

    void ScanDiskInfo(bool scanTop);
    void LoadList(void);
    void AddToList(CListCtrl* pListView, DiskImgLib::DiskImg* pDiskImg,
        DiskImgLib::DiskFS* pDiskFS, int* pIndex);
    bool GetSelectedDisk(DiskImgLib::DiskImg** ppDstImg,
        DiskImgLib::DiskFS** ppDiskFS);

    // Load images to be used in the list.  Apparently this must be called
    // before we try to load any header images.
    void LoadListImages(void) {
        if (!fListImageList.Create(IDB_VOL_PICS, 16, 1, CLR_DEFAULT))
            WMSG0("GLITCH: list image create failed\n");
        fListImageList.SetBkColor(::GetSysColor(COLOR_WINDOW));
    }
    enum {  // defs for IDB_VOL_PICS
        kListIconVolume = 0,
        kListIconSubVolume = 1,
    };
    CImageList      fListImageList;

    DiskImgLib::DiskFS*     fpDiskFS;

    ModelessDialog* fpWaitDlg;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_VOLUMECOPYDIALOG_H*/
