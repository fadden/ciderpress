/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Disk Edit "open file" dialog.
 *
 * If the dialog returns with IDOK, "fName" will be a string at least 1
 * character long.
 *
 * Currently this is a trivial edit box that asks for a name.  In the future
 * this might present a list or tree view to choose from.
 *
 * NOTE: we probably want to have a read-only flag here, defaulted to the
 * state of the sector editor.  The read-only state of the underlying FS
 * doesn't matter, since we're writing sectors, not really editing files.
 */
#ifndef __DEFILEDIALOG__
#define __DEFILEDIALOG__

#include "resource.h"
#include "../diskimg/DiskImg.h"
using namespace DiskImgLib;


/*
 * Class declaration.  Nothing special.
 */
class DEFileDialog : public CDialog {
public:
	DEFileDialog(CWnd* pParentWnd = NULL) : CDialog(IDD_DEFILE, pParentWnd)
	{
		fOpenRsrcFork = false;
		fName = "";
	}
	virtual ~DEFileDialog(void) {}

	void Setup(DiskFS* pDiskFS) {
		fpDiskFS = pDiskFS;
	}

	CString		fName;
	int			fOpenRsrcFork;

protected:
	// overrides
	virtual BOOL OnInitDialog(void);
	virtual void DoDataExchange(CDataExchange* pDX);

	afx_msg virtual void OnChange(void);
	afx_msg virtual BOOL OnHelpInfo(HELPINFO* lpHelpInfo);

private:
	DiskFS*		fpDiskFS;

	DECLARE_MESSAGE_MAP()
};

#endif /*__DEFILEDIALOG__*/