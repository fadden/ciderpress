/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Declarations for "rename volume" dialog.
 */
#ifndef __RENAMEVOLUME__
#define __RENAMEVOLUME__

#include "DiskFSTree.h"
#include "resource.h"

class DiskArchive;

/*
 * Get a pointer to the DiskFS that we're altering, and a valid string for
 * the new volume name.
 */
class RenameVolumeDialog : public CDialog {
public:
	RenameVolumeDialog(CWnd* pParentWnd = NULL) :
		CDialog(IDD_RENAME_VOLUME, pParentWnd)
	{
		fpArchive = nil;
	}
	virtual ~RenameVolumeDialog(void) {}

	const DiskArchive*	fpArchive;
	CString				fNewName;
	DiskImgLib::DiskFS*	fpChosenDiskFS;

protected:
	// overrides
	virtual BOOL OnInitDialog(void);
	virtual void DoDataExchange(CDataExchange* pDX);

	afx_msg void OnSelChanged(NMHDR* pnmh, LRESULT* pResult);
	afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo);
	afx_msg void OnHelp(void);

	DiskFSTree	fDiskFSTree;

private:

	DECLARE_MESSAGE_MAP()
};

#endif /*__RENAMEVOLUME__*/
