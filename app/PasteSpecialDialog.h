/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Paste Special dialog.
 */
#ifndef __PASTESPECIALDIALOG__
#define __PASTESPECIALDIALOG__

#include "resource.h"

/*
 * Simple dialog with a radio button.
 */
class PasteSpecialDialog : public CDialog {
public:
	PasteSpecialDialog(CWnd* pParentWnd = nil) :
	  CDialog(IDD_PASTE_SPECIAL, pParentWnd),
	  fPasteHow(kPastePaths)
	  {}
	 virtual ~PasteSpecialDialog() {}

	 /* right now this is boolean, but that may change */
	 /* (e.g. "paste clipboard contents into new text file") */
	 enum {
		kPasteUnknown = 0,
		kPastePaths,
		kPasteNoPaths,
	 };
	 int	fPasteHow;

protected:
	//virtual BOOL OnInitDialog(void);
	void DoDataExchange(CDataExchange* pDX);

	//DECLARE_MESSAGE_MAP()
};

#endif /*__PASTESPECIALDIALOG__*/