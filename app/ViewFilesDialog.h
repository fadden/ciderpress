/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Class for the "view files" dialog box.
 */
#ifndef APP_VIEWFILESDIALOG_H
#define APP_VIEWFILESDIALOG_H

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
        CDialog(IDD_FILE_VIEWER, pParentWnd),
        fpSelSet(NULL),
        fpHolder(NULL),
        fpOutput(NULL),
        fPointSize(0),
        fNoWrapText(false),
        fBusy(false),
        fFirstResize(false),
        fpRichEditOle(NULL),
        fpFindDialog(NULL),
        fFindDown(true),
        fFindMatchCase(false),
        fFindMatchWholeWord(false)
    {}
    virtual ~ViewFilesDialog(void) {
        delete fpHolder;
        delete fpOutput;
        // Windows will handle destruction of fpFindDialog (child window)
    }

    void SetSelectionSet(SelectionSet* pSelSet) { fpSelSet = pSelSet; }

    CString GetTextTypeFace(void) const { return fTypeFace; }
    void SetTextTypeFace(const WCHAR* name) { fTypeFace = name; }
    int GetTextPointSize(void) const { return fPointSize; }
    void SetTextPointSize(int size) { fPointSize = size; }
    //bool GetNoWrapText(void) const { return fNoWrapText; }
    void SetNoWrapText(bool val) { fNoWrapText = val; }

protected:
    /*
     * Window creation.  Stuff the desired text into the RichEdit box.
     */
    virtual BOOL OnInitDialog(void) override;

    /*
     * Override OnOK/OnCancel so we don't bail out while we're in the middle of
     * loading something.  It would actually be kind of nice to be able to do
     * so, so someday we should make the "cancel" button work, or perhaps allow
     * prev/next to skip over the thing being loaded. "TO DO"
     */
    virtual void OnOK(void) override;
    virtual void OnCancel(void) override;

    virtual void DoDataExchange(CDataExchange* pDX) override;

    /*
     * Window creation stuff.  Set the icon and the "gripper".
     */
    afx_msg int OnCreate(LPCREATESTRUCT lpcs);

    /*
     * Window is going away.  Save the current size.
     */
    afx_msg void OnDestroy(void);

    /*
     * When the window resizes, we have to tell the edit box to expand, and
     * rearrange the controls inside it.
     */
    afx_msg void OnSize(UINT nType, int cx, int cy);

    /*
     * Restrict the minimum window size to something reasonable.
     */
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
    afx_msg void OnHelp(void) {
        MyApp::HandleHelp(this, HELP_TOPIC_FILE_VIEWER);
    }
    //afx_msg void OnFviewWrap(void);
    afx_msg LRESULT OnFindDialogMessage(WPARAM wParam, LPARAM lParam);

private:
    /*
     * Adjust the positions and sizes of the controls.
     *
     * This relies on MinMaxInfo to guarantee that nothing falls off an edge.
     */
    void ShiftControls(int deltaX, int deltaY);

    //void MoveControl(int id, int deltaX, int deltaY);
    //void StretchControl(int id, int deltaX, int deltaY);
    void NewFontSelected(bool resetBold);

    /*
     * Determines whether the specified part is an empty fork/comment.
     */
    bool IsSourceEmpty(const GenericEntry* pEntry,
        ReformatHolder::ReformatPart part);

    /*
     * Display a buffer of text in the RichEdit control.
     *
     * The RichEdit dialog will hold its own copy of the data, so "pHolder" can
     * be safely destroyed after this returns.
     *
     * "fileName" is for display only.  "zeroSourceLen" allows the function to
     * tell the difference between an empty file and a non-empty file that
     * generated empty output.
     */
    void DisplayText(const WCHAR* fileName, bool zeroSourceLen);

    /*
     * Set up the fpHolder.  Does not reformat the data, just loads the source
     * material and runs the applicability tests.
     *
     * Returns 0 on success, -1 on failure.
     */
    int ReformatPrep(GenericEntry* pEntry);

    /*
     * Reformat a file.
     *
     * Returns 0 if the file was reformatted, -1 if not
     */
    int Reformat(const GenericEntry* pEntry,
        ReformatHolder::ReformatPart part, ReformatHolder::ReformatID id);

    /*
     * Configure the radio buttons that determine which part to view, enabling
     * only those that make sense.
     *
     * Try to keep the previously-set button set.
     *
     * If "pEntry" is NULL, all buttons are disabled (useful for first-time
     * initialization).
     */
    void ConfigurePartButtons(const GenericEntry* pEntry);

    /*
     * Figure out which part of the file is selected (data/rsrc/comment).
     *
     * If no part is selected, throws up its hands and returns kPartData.
     */
    ReformatHolder::ReformatPart GetSelectedPart(void);

    void ForkSelectCommon(ReformatHolder::ReformatPart part);

    /*
     * Set up the entries in the drop box based on the "applicable" array in
     * fpHolder.  The set of values is different for each part of the file.
     *
     * Returns the default reformatter ID.  This is always entry #0.
     */
    ReformatHolder::ReformatID ConfigureFormatSel(
        ReformatHolder::ReformatPart part);

    /*
     * Return the combo box index for the entry whose "data" field matches "val".
     *
     * Returns -1 if the entry couldn't be found.
     */
    int FindByVal(CComboBox* pCombo, DWORD val);

    /*
     * Enable or disable all of the format selection buttons.
     */
    void EnableFormatSelection(BOOL enable);

    /*
     * Find the next ocurrence of the specified string.
     */
    void FindNext(const WCHAR* str, bool down, bool matchCase,
        bool wholeWord);

    // pointer to main window, so we can ask for text to view
    //MainWindow*       fpMainWindow;

    // stuff to display
    SelectionSet*   fpSelSet;

    // edit control
    CRichEditCtrl   fEditCtrl;

    // currently loaded file
    ReformatHolder* fpHolder;

    // most recent conversion
    ReformatOutput* fpOutput;

    // current title of window
    CString         fTitle;

    // used to display a "gripper" in the bottom right of the dialog
    CGripper        fGripper;

    // last size of the window, so we can shift things around
    CRect           fLastWinSize;

    // font characteristics
    CString         fTypeFace;      // name of font
    int             fPointSize;     // size, in points

    // do we want to scroll or wrap?
    bool            fNoWrapText;

    // the message pump in the progress updater can cause NufxLib reentrancy
    // (alternate solution: disable the window while we load stuff)
    bool            fBusy;

    // this is *really* annoying
    bool            fFirstResize;

    // used for stuffing images in; points at something inside RichEdit ctrl
    IRichEditOle*   fpRichEditOle;

    CFindReplaceDialog* fpFindDialog;
    CString         fFindLastStr;
    bool            fFindDown;
    bool            fFindMatchCase;
    bool            fFindMatchWholeWord;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_VIEWFILESDIALOG_H*/
