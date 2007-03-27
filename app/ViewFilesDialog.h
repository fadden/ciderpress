/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Class for the "view files" dialog box.
 */
#ifndef __VIEWFILESDIALOG__
#define __VIEWFILESDIALOG__

#include "GenericArchive.h"
#include "resource.h"

class MainWindow;

/*
 * Implementation of the "view files" dialog box.
 *
 * The default window size is actually defined over in Preferences.cpp.
 * The window size is a "sticky" pref (i.e. not stored in registry).
 */
class ViewFilesDialog : public CDialog {
public:
	ViewFilesDialog(CWnd* pParentWnd = NULL) :
		CDialog(IDD_FILE_VIEWER, pParentWnd)
	{
		//fpMainWindow = nil;
		fpSelSet = nil;
		fpHolder = nil;
		fpOutput = nil;
		fTypeFace = "";
		fPointSize = 0;
		fNoWrapText = false;
		fBusy = false;
		fpRichEditOle = nil;
		fFirstResize = false;

		fpFindDialog = nil;
		fFindDown = false;
		fFindMatchCase = false;
		fFindMatchWholeWord = false;
	}
	virtual ~ViewFilesDialog(void) {
		delete fpHolder;
		delete fpOutput;
		// Windows will handle destruction of fpFindDialog (child window)
	}

	void SetSelectionSet(SelectionSet* pSelSet) { fpSelSet = pSelSet; }

	CString GetTextTypeFace(void) const { return fTypeFace; }
	void SetTextTypeFace(const char* name) { fTypeFace = name; }
	int GetTextPointSize(void) const { return fPointSize; }
	void SetTextPointSize(int size) { fPointSize = size; }
	//bool GetNoWrapText(void) const { return fNoWrapText; }
	void SetNoWrapText(bool val) { fNoWrapText = val; }

protected:
	// overrides
	virtual BOOL OnInitDialog(void);
	virtual void OnOK(void);
	virtual void OnCancel(void);
	virtual void DoDataExchange(CDataExchange* pDX);

	afx_msg int OnCreate(LPCREATESTRUCT lpcs);
	afx_msg void OnDestroy(void);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* pMMI);

	afx_msg void OnFviewNext(void);
	afx_msg void OnFviewPrev(void);
	afx_msg void OnFviewFont(void);
	afx_msg void OnFviewPrint(void);
	afx_msg void OnFviewFind(void);
	afx_msg void OnFviewData(void);
	afx_msg void OnFviewRsrc(void);
	afx_msg void OnFviewCmmt(void);
	afx_msg void OnFviewFmtBest(void);
	afx_msg void OnFviewFmtHex(void);
	afx_msg void OnFviewFmtRaw(void);
	afx_msg void OnFormatSelChange(void);
	afx_msg void OnHelp(void);
	//afx_msg void OnFviewWrap(void);
	afx_msg LRESULT OnFindDialogMessage(WPARAM wParam, LPARAM lParam);

private:
	void ShiftControls(int deltaX, int deltaY);
	//void MoveControl(int id, int deltaX, int deltaY);
	//void StretchControl(int id, int deltaX, int deltaY);
	void NewFontSelected(bool resetBold);
	void DisplayText(const char* fileName);
	int ReformatPrep(GenericEntry* pEntry);
	int Reformat(const GenericEntry* pEntry,
		ReformatHolder::ReformatPart part, ReformatHolder::ReformatID id);
	void ConfigurePartButtons(const GenericEntry* pEntry);
	ReformatHolder::ReformatPart GetSelectedPart(void);
	void ForkSelectCommon(ReformatHolder::ReformatPart part);
	ReformatHolder::ReformatID ConfigureFormatSel(
		ReformatHolder::ReformatPart part);
	int FindByVal(CComboBox* pCombo, DWORD val);
	void EnableFormatSelection(BOOL enable);
	void FindNext(const char* str, bool down, bool matchCase,
		bool wholeWord);

	// pointer to main window, so we can ask for text to view
	//MainWindow*		fpMainWindow;

	// stuff to display
	SelectionSet*	fpSelSet;

	// edit control
	CRichEditCtrl	fEditCtrl;

	// currently loaded file
	ReformatHolder*	fpHolder;

	// most recent conversion
	ReformatOutput* fpOutput;

	// current title of window
	CString			fTitle;

	// used to display a "gripper" in the bottom right of the dialog
	CGripper		fGripper;

	// last size of the window, so we can shift things around
	CRect			fLastWinSize;

	// font characteristics
	CString			fTypeFace;		// name of font
	int				fPointSize;		// size, in points

	// do we want to scroll or wrap?
	bool			fNoWrapText;

	// the message pump in the progress updater can cause NufxLib reentrancy
	// (alternate solution: disable the window while we load stuff)
	bool			fBusy;

	// this is *really* annoying
	bool			fFirstResize;

	// used for stuffing images in; points at something inside RichEdit ctrl
	IRichEditOle*	fpRichEditOle;

	CFindReplaceDialog*	fpFindDialog;
	CString			fFindLastStr;
	bool			fFindDown;
	bool			fFindMatchCase;
	bool			fFindMatchWholeWord;

	DECLARE_MESSAGE_MAP()
};

#endif /*__VIEWFILESDIALOG__*/