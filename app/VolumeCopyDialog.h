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
        fpDiskImg(NULL),
        fpDiskFS(NULL),
        fpWaitDlg(NULL)
    {}
    ~VolumeCopyDialog(void) { assert(fpDiskFS == NULL); }

    /* disk image to work with; we don't own it */
    DiskImgLib::DiskImg*    fpDiskImg;
    /* path name of input disk image or volume; mainly for display */
    CString     fPathName;

protected:
    virtual BOOL OnInitDialog(void) override;
    virtual void OnOK(void) override;
    virtual void OnCancel(void) override;

    void Cleanup(void);
    
    enum { WMU_DIALOG_READY = WM_USER+2 };

    /*
     * Something changed in the list.  Update the buttons.
     */
    afx_msg void OnListChange(NMHDR* pNotifyStruct, LRESULT* pResult);

    /*
     * User pressed the "copy to file" button.  Copy the selected partition out to
     * a file on disk.
     */
    afx_msg void OnCopyToFile(void);

    /*
     * User pressed the "copy from file" button.  Copy a file over the selected
     * partition.  We may need to reload the main window after this completes.
     */
    afx_msg void OnCopyFromFile(void);

    afx_msg void OnHelp(void) {
        MyApp::HandleHelp(this, HELP_TOPIC_VOLUME_COPIER);
    }

    /*
     * When the focus changes, e.g. after dialog construction completes, see if
     * we have a modeless dialog lurking about.
     */
    afx_msg LONG OnDialogReady(UINT, LONG);

    /*
     * (Re-)scan the disk image and any sub-volumes.
     */
    void ScanDiskInfo(bool scanTop);

    /*
     * (Re-)load the volume and sub-volumes into the list.
     *
     * We currently only look at the first level of sub-volumes.  We're not
     * really set up to display a hierarchy in the list view.  Very few people
     * will ever need to access a sub-sub-volume in this way, so it's not
     * worth sorting it out.
     */
    void LoadList(void);

    /*
     * Create an entry for a diskimg/diskfs pair.
     */
    void AddToList(CListCtrl* pListView, DiskImgLib::DiskImg* pDiskImg,
        DiskImgLib::DiskFS* pDiskFS, int* pIndex);

    /*
     * Recover the DiskImg and DiskFS pointers for the volume or sub-volume
     * currently selected in the list.
     *
     * Returns "true" on success, "false" on failure.
     */
    bool GetSelectedDisk(DiskImgLib::DiskImg** ppDstImg,
        DiskImgLib::DiskFS** ppDiskFS);

    // Load images to be used in the list.  Apparently this must be called
    // before we try to load any header images.
    void LoadListImages(void) {
        if (!fListImageList.Create(IDB_VOL_PICS, 16, 1, CLR_DEFAULT))
            LOGI("GLITCH: list image create failed");
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
