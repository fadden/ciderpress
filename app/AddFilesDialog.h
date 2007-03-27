/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * File selection dialog, a sub-class of "Open" that allows multiple selection
 * of both files and directories.
 */
#ifndef __ADDFILESDIALOG__
#define __ADDFILESDIALOG__

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
		SelectFilesDialog("IDD_ADD_FILES", pParentWnd)
	{
		SetWindowTitle(_T("Add Files..."));
		fStoragePrefix = "";
		fStoragePrefixEnable = true;
		fIncludeSubfolders = FALSE;
		fStripFolderNames = FALSE;
		fStripFolderNamesEnable = true;
		fOverwriteExisting = FALSE;
		fTypePreservation = 0;
		fConvEOL = 0;
		fConvEOLEnable = true;

		fAcceptButtonID = IDC_SELECT_ACCEPT;

		fpTargetDiskFS = nil;
		//fpTargetSubdir = nil;
		fpDiskImg = nil;
	}
	virtual ~AddFilesDialog(void) {}

	/* values from dialog */
	CString	fStoragePrefix;
	bool	fStoragePrefixEnable;
	BOOL	fIncludeSubfolders;
	BOOL	fStripFolderNames;
	bool	fStripFolderNamesEnable;
	BOOL	fOverwriteExisting;

	enum { kPreserveNone = 0, kPreserveTypes, kPreserveAndExtend };
	int		fTypePreservation;

	enum { kConvEOLNone = 0, kConvEOLType, kConvEOLAuto, kConvEOLAll };
	int		fConvEOL;
	bool	fConvEOLEnable;

	/* carryover from ChooseAddTargetDialog */
	DiskImgLib::DiskFS*		fpTargetDiskFS;
	//DiskImgLib::A2File*		fpTargetSubdir;

	/* kluge; we carry this around for the benefit of AddDisk */
	DiskImgLib::DiskImg*	fpDiskImg;

private:
	virtual bool MyDataExchange(bool saveAndValidate);
	virtual void ShiftControls(int deltaX, int deltaY);
	virtual UINT MyOnCommand(WPARAM wParam, LPARAM lParam);

	void OnIDHelp(void);
	bool ValidateStoragePrefix(void);

	//DECLARE_MESSAGE_MAP()
};

#endif /*__ADDFILESDIALOG__*/