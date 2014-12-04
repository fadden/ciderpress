/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * File selection dialog, a sub-class of "Open" that allows multiple selection
 * of both files and directories.
 */
#ifndef APP_ADDFILESDIALOG_H
#define APP_ADDFILESDIALOG_H

#include "../diskimg/DiskImg.h"
#include "../util/UtilLib.h"
#include "resource.h"

/*
 * Choose files and folders to add.
 *
 * This gets passed down through the file add stuff, so it needs to carry some
 * extra data along as well.
 */
class AddFilesDialog : public SelectFilesDialog {
public:
    AddFilesDialog(CWnd* pParentWnd = NULL) :
        SelectFilesDialog(L"IDD_ADD_FILES", true, pParentWnd)
    {
        SetWindowTitle(L"Add Files...");
        fStoragePrefix = "";
        fStoragePrefixEnable = true;
        fIncludeSubfolders = FALSE;
        fStripFolderNames = FALSE;
        fStripFolderNamesEnable = true;
        fOverwriteExisting = FALSE;
        fTypePreservation = 0;
        fConvEOL = 0;
        fConvEOLEnable = true;

        fpTargetDiskFS = NULL;
        //fpTargetSubdir = NULL;
        fpDiskImg = NULL;
    }
    virtual ~AddFilesDialog(void) {}

    /* values from dialog */
    CString fStoragePrefix;
    bool    fStoragePrefixEnable;
    BOOL    fIncludeSubfolders;
    BOOL    fStripFolderNames;
    bool    fStripFolderNamesEnable;
    BOOL    fOverwriteExisting;

    enum { kPreserveNone = 0, kPreserveTypes, kPreserveAndExtend };
    int     fTypePreservation;

    enum { kConvEOLNone = 0, kConvEOLType, kConvEOLAuto, kConvEOLAll };
    int     fConvEOL;
    bool    fConvEOLEnable;

    /* carryover from ChooseAddTargetDialog */
    DiskImgLib::DiskFS*     fpTargetDiskFS;
    //DiskImgLib::A2File*       fpTargetSubdir;

    /* kluge; we carry this around for the benefit of AddDisk */
    DiskImgLib::DiskImg*    fpDiskImg;

private:
    virtual bool MyDataExchange(bool saveAndValidate) override;

    // User hit the Help button.
    virtual void HandleHelp() override;

    // Make sure the storage prefix they entered is valid.
    bool ValidateStoragePrefix();

    //DECLARE_MESSAGE_MAP()
};

#endif /*APP_ADDFILESDIALOG_H*/
