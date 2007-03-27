/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Classes to support the Preferences property pages.
 */
#ifndef __PREFSDIALOG__
#define __PREFSDIALOG__

#include "Preferences.h"
#include "../util/UtilLib.h"
#include "resource.h"

/*
 * The "general" page, which controls how we display information to the user.
 */
class PrefsGeneralPage : public CPropertyPage
{
public:
	PrefsGeneralPage(void) :
	  CPropertyPage(IDD_PREF_GENERAL),
	  fReady(false),
	  fMimicShrinkIt(FALSE),
	  fBadMacSHK(FALSE),
	  fReduceSHKErrorChecks(FALSE),
	  fCoerceDOSFilenames(FALSE),
	  fSpacesToUnder(FALSE),
	  fDefaultsPushed(FALSE),
	  fOurAssociations(nil)
		{}
	virtual ~PrefsGeneralPage(void) {
		delete[] fOurAssociations;
	}

	bool	fReady;

	// fields on this page
	BOOL	fColumn[kNumVisibleColumns];
	BOOL	fMimicShrinkIt;
	BOOL	fBadMacSHK;
	BOOL	fReduceSHKErrorChecks;
	BOOL	fCoerceDOSFilenames;
	BOOL	fSpacesToUnder;
	BOOL	fPasteJunkPaths;
	BOOL	fBeepOnSuccess;
	BOOL	fDefaultsPushed;

	// initialized if we opened the file associations edit page
	bool*	fOurAssociations;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

	afx_msg void OnChange(void);
	afx_msg void OnChangeRange(UINT);
	afx_msg void OnDefaults(void);
	afx_msg void OnAssociations(void);
	afx_msg LONG OnHelp(UINT wParam, LONG lParam);
	afx_msg LONG OnCommandHelp(UINT wParam, LONG lParam);

	DECLARE_MESSAGE_MAP()
};

/*
 * The "disk image" page, for selecting disk image preferences.
 */
class PrefsDiskImagePage : public CPropertyPage
{
public:
	PrefsDiskImagePage(void) :
	  CPropertyPage(IDD_PREF_DISKIMAGE),
	  fReady(false),
	  fQueryImageFormat(FALSE),
	  fOpenVolumeRO(FALSE),
	  fProDOSAllowLower(FALSE),
	  fProDOSUseSparse(FALSE)
	  {}

	bool	fReady;

	BOOL	fQueryImageFormat;
	BOOL	fOpenVolumeRO;
	BOOL	fOpenVolumePhys0;
	BOOL	fProDOSAllowLower;
	BOOL	fProDOSUseSparse;

protected:
	virtual BOOL OnInitDialog(void);
	virtual void DoDataExchange(CDataExchange* pDX);

	afx_msg void OnChange(void);
	//afx_msg void OnChangeRange(UINT);
	afx_msg LONG OnHelp(UINT wParam, LONG lParam);
	afx_msg LONG OnCommandHelp(UINT wParam, LONG lParam);

	DECLARE_MESSAGE_MAP()
};

/*
 * The "compression" page, which lets the user choose a default compression
 * method.
 */
class PrefsCompressionPage : public CPropertyPage
{
public:
	PrefsCompressionPage(void) :
	  CPropertyPage(IDD_PREF_COMPRESSION), fReady(false)
	  {}

	bool	fReady;

	int		fCompressType;		// radio button index

protected:
	virtual BOOL OnInitDialog(void);
	virtual void DoDataExchange(CDataExchange* pDX);

	afx_msg void OnChangeRange(UINT);
	afx_msg LONG OnHelp(UINT wParam, LONG lParam);
	afx_msg LONG OnCommandHelp(UINT wParam, LONG lParam);

private:
	void DisableWnd(int id);

	DECLARE_MESSAGE_MAP()
};

/*
 * The "fview" page, for selecting preferences for the internal file viewer.
 */
class PrefsFviewPage : public CPropertyPage
{
public:
	PrefsFviewPage(void) :
	  CPropertyPage(IDD_PREF_FVIEW), fReady(false)
	{}
	bool	fReady;

	BOOL	fEOLConvRaw;
	BOOL	fNoWrapText;
	BOOL	fHighlightHexDump;
	BOOL	fHighlightBASIC;
	BOOL	fConvDisasmOneByteBrkCop;
	BOOL	fConvHiResBlackWhite;
	int		fConvDHRAlgorithm;		// drop list

	BOOL	fConvTextEOL_HA;
	BOOL	fConvCPMText;
	BOOL	fConvPascalText;
	BOOL	fConvPascalCode;
	BOOL	fConvApplesoft;
	BOOL	fConvInteger;
	BOOL	fConvGWP;
	BOOL	fConvText8;
	BOOL	fConvAWP;
	BOOL	fConvADB;
	BOOL	fConvASP;
	BOOL	fConvSCAssem;
	BOOL	fConvDisasm;

	BOOL	fConvHiRes;
	BOOL	fConvDHR;
	BOOL	fConvSHR;
	BOOL	fConvPrintShop;
	BOOL	fConvMacPaint;
	BOOL	fConvProDOSFolder;
	BOOL	fConvResources;
	BOOL	fRelaxGfxTypeCheck;

	UINT	fMaxViewFileSizeKB;

protected:
	virtual BOOL OnInitDialog(void);
	virtual void DoDataExchange(CDataExchange* pDX);

	afx_msg void OnChange(void);
	afx_msg void OnChangeRange(UINT);
	afx_msg LONG OnHelp(UINT wParam, LONG lParam);
	afx_msg LONG OnCommandHelp(UINT wParam, LONG lParam);

	DECLARE_MESSAGE_MAP()
};

/*
 * The "compression" page, which lets the user choose a default compression
 * method for NuFX archives.
 */
class PrefsFilesPage : public CPropertyPage
{
public:
	PrefsFilesPage(void) :
	  CPropertyPage(IDD_PREF_FILES), fReady(false)
	  {}

	bool	fReady;

	CString	fTempPath;
	CString	fExtViewerExts;

protected:
	virtual BOOL OnInitDialog(void);
	virtual void DoDataExchange(CDataExchange* pDX);

	afx_msg void OnChange(void);
	afx_msg void OnChooseFolder(void);
	afx_msg LONG OnHelp(UINT wParam, LONG lParam);
	afx_msg LONG OnCommandHelp(UINT wParam, LONG lParam);

	MyBitmapButton	fChooseFolderButton;

	DECLARE_MESSAGE_MAP()
};


/*
 * Property sheet that wraps around the preferences pages.
 */
class PrefsSheet : public CPropertySheet
{
public:
	PrefsSheet(CWnd* pParentWnd = NULL);

	PrefsGeneralPage		fGeneralPage;
	PrefsDiskImagePage		fDiskImagePage;
	PrefsCompressionPage	fCompressionPage;
	PrefsFviewPage			fFviewPage;
	PrefsFilesPage			fFilesPage;

protected:
	BOOL OnNcCreate(LPCREATESTRUCT cs);

	afx_msg void OnApplyNow();
	LONG OnHelp(UINT wParam, LONG lParam);
	void OnIDHelp(void);

	DECLARE_MESSAGE_MAP()
};

#endif /*__PREFSDIALOG__*/