/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Dialog asking the user to confirm certain details of a disk image.
 */
#ifndef __IMAGEFORMATDIALOG__
#define __IMAGEFORMATDIALOG__

//#include <afxwin.h>
#include "resource.h"
#include "../diskimg/DiskImg.h"
using namespace DiskImgLib;

/*
 * The default values can be initialized individually or from a prepped
 * DiskImg structure.
 */
class ImageFormatDialog : public CDialog {
public:
	ImageFormatDialog(CWnd* pParentWnd = NULL) :
		CDialog(IDD_DECONF, pParentWnd)
	{
		fInitialized = false;
		fFileSource = "";
		fAllowUnknown = false;
		fOuterFormat = DiskImg::kOuterFormatUnknown;
		fFileFormat = DiskImg::kFileFormatUnknown;
		fPhysicalFormat = DiskImg::kPhysicalFormatUnknown;
		fSectorOrder = DiskImg::kSectorOrderUnknown;
		fFSFormat = DiskImg::kFormatUnknown;
		fDisplayFormat = kShowAsBlocks;

		fQueryDisplayFormat = true;
		fAllowGenericFormats = true;
		fHasSectors = fHasBlocks = fHasNibbles = false;
	}

	// initialize values from a DiskImg
	void InitializeValues(const DiskImg* pImg);

	bool					fInitialized;
	CString					fFileSource;
	bool					fAllowUnknown;	// allow "unknown" choice?

	DiskImg::OuterFormat	fOuterFormat;
	DiskImg::FileFormat		fFileFormat;
	DiskImg::PhysicalFormat	fPhysicalFormat;
	DiskImg::SectorOrder	fSectorOrder;
	DiskImg::FSFormat		fFSFormat;

	enum { kShowAsBlocks=0, kShowAsSectors=1, kShowAsNibbles=2 };
	int						fDisplayFormat;

	void SetQueryDisplayFormat(bool val) { fQueryDisplayFormat = val; }
	void SetAllowGenericFormats(bool val) { fAllowGenericFormats = val; }

protected:
	//virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog(void);
	void OnOK(void);
	afx_msg virtual void OnHelp(void);
	afx_msg virtual BOOL OnHelpInfo(HELPINFO* lpHelpInfo);

	struct ConvTable;
	void LoadComboBoxes(void);
	void LoadComboBox(int boxID, const ConvTable* pTable, int dflt);
	int ConvComboSel(int boxID, const ConvTable* pTable);

	bool					fQueryDisplayFormat;
	bool					fAllowGenericFormats;
	bool					fHasSectors;
	bool					fHasBlocks;
	bool					fHasNibbles;

	DECLARE_MESSAGE_MAP()
};

#endif /*__IMAGEFORMATDIALOG__*/