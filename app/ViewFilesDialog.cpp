/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for the "view files" dialog box.
 */
#include "stdafx.h"
#include "ViewFilesDialog.h"
#include "Main.h"
#include "Print.h"
#include "HelpTopics.h"
#include "../util/UtilLib.h"


/*
 * ===========================================================================
 *      ViewFilesDialog
 * ===========================================================================
 */

static const UINT gFindReplaceID = RegisterWindowMessage(FINDMSGSTRING);

BEGIN_MESSAGE_MAP(ViewFilesDialog, CDialog)
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_REGISTERED_MESSAGE(gFindReplaceID, OnFindDialogMessage)
    ON_COMMAND(IDC_FVIEW_NEXT, OnFviewNext)
    ON_COMMAND(IDC_FVIEW_PREV, OnFviewPrev)
    ON_COMMAND(IDC_FVIEW_FONT, OnFviewFont)
    ON_COMMAND(IDC_FVIEW_PRINT, OnFviewPrint)
    ON_COMMAND(IDC_FVIEW_FIND, OnFviewFind)
    ON_BN_CLICKED(IDC_FVIEW_DATA, OnFviewData)
    ON_BN_CLICKED(IDC_FVIEW_RSRC, OnFviewRsrc)
    ON_BN_CLICKED(IDC_FVIEW_CMMT, OnFviewCmmt)
    ON_COMMAND(IDC_FVIEW_FMT_BEST, OnFviewFmtBest)
    ON_COMMAND(IDC_FVIEW_FMT_HEX, OnFviewFmtHex)
    ON_COMMAND(IDC_FVIEW_FMT_RAW, OnFviewFmtRaw)
    ON_CBN_SELCHANGE(IDC_FVIEW_FORMATSEL, OnFormatSelChange)
    ON_COMMAND(IDHELP, OnHelp)
END_MESSAGE_MAP()

/*
 * Window creation.  Stuff the desired text into the RichEdit box.
 */
BOOL
ViewFilesDialog::OnInitDialog(void)
{
    WMSG0("Now in VFD OnInitDialog!\n");

    ASSERT(fpSelSet != nil);

    /* delete dummy control and insert our own with modded styles */
    CRichEditCtrl* pEdit = (CRichEditCtrl*)GetDlgItem(IDC_FVIEW_EDITBOX);
    ASSERT(pEdit != nil);
    CRect rect;
    pEdit->GetWindowRect(&rect);
    pEdit->DestroyWindow();
    ScreenToClient(&rect);

    DWORD styles = ES_MULTILINE | ES_AUTOVSCROLL |  ES_READONLY | 
                    WS_BORDER | WS_VSCROLL | WS_VISIBLE | WS_TABSTOP |
                    ES_NOHIDESEL;
    if (fNoWrapText)
        styles |= ES_AUTOHSCROLL | WS_HSCROLL;
    fEditCtrl.Create(styles, rect, this, IDC_FVIEW_EDITBOX);
    fEditCtrl.SetFocus();
    /*
     * HEY: I think we can do this with pEdit->ShowScrollBar(SB_BOTH) !!
     * Could also use GetWindowLong/SetWindowLong to change the window style;
     * probably need a SetWindowPos to cause changes to flush.
     */


    /*
     * We want to adjust the size of the window to match the last size used.
     * However, if we do this after creating the edit dialog but before it
     * actually has data to display, then when we stuff data into it the
     * scroll bar goodies are all out of whack.
     */
    fFirstResize = true;
#if 0
    const Preferences* pPreferences = GET_PREFERENCES();
    long width = pPreferences->GetFileViewerWidth();
    long height = pPreferences->GetFileViewerHeight();
    CRect fullRect;
    GetWindowRect(&fullRect);
    WMSG2(" VFD pre-size %dx%d\n", fullRect.Width(), fullRect.Height());
    fullRect.right = fullRect.left + width;
    fullRect.bottom = fullRect.top + height;
    MoveWindow(fullRect, FALSE);
#endif

    // This invokes UpdateData, which calls DoDataExchange, which leads to
    // the StreamIn call.  So don't do this until everything else is ready.
    CDialog::OnInitDialog();

    WMSG0("VFD OnInitDialog done\n");
    return FALSE;   // don't let Windows set the focus
}


/*
 * Window creation stuff.  Set the icon and the "gripper".
 */
int
ViewFilesDialog::OnCreate(LPCREATESTRUCT lpcs)
{
    WMSG0("VFD OnCreate\n");

    HICON hIcon;
    hIcon = ::AfxGetApp()->LoadIcon(IDI_FILE_VIEWER);
    SetIcon(hIcon, TRUE);

    GetClientRect(&fLastWinSize);

    CRect initRect(fLastWinSize);
    initRect.left = initRect.right - ::GetSystemMetrics(SM_CXVSCROLL);
    initRect.top  = initRect.bottom - ::GetSystemMetrics(SM_CYHSCROLL);
    fGripper.Create(WS_CHILD | WS_VISIBLE |
                    SBS_SIZEBOX | SBS_SIZEBOXBOTTOMRIGHTALIGN | SBS_SIZEGRIP,
        initRect, this, AFX_IDW_SIZE_BOX);

    WMSG0("VFD OnCreate done\n");
    return 0;
}

/*
 * Window is going away.  Save the current size.
 */
void
ViewFilesDialog::OnDestroy(void)
{
    Preferences* pPreferences = GET_PREFERENCES_WR();
    CRect rect;
    GetWindowRect(&rect);

    pPreferences->SetPrefLong(kPrFileViewerWidth, rect.Width());
    pPreferences->SetPrefLong(kPrFileViewerHeight, rect.Height());

    CDialog::OnDestroy();
}

/*
 * Override OnOK/OnCancel so we don't bail out while we're in the middle of
 * loading something.  It would actually be kind of nice to be able to do
 * so, so someday we should make the "cancel" button work, or perhaps allow
 * prev/next to skip over the thing being loaded. "TO DO"
 */
void
ViewFilesDialog::OnOK(void)
{
    if (fBusy)
        MessageBeep(-1);
    else {
        CRect rect;

        GetWindowRect(&rect);
        WMSG2(" VFD size now %dx%d\n", rect.Width(), rect.Height());

        CDialog::OnOK();
    }
}
void
ViewFilesDialog::OnCancel(void)
{
    if (fBusy)
        MessageBeep(-1);
    else
        CDialog::OnCancel();
}


/*
 * Restrict the minimum window size to something reasonable.
 */
void
ViewFilesDialog::OnGetMinMaxInfo(MINMAXINFO* pMMI)
{
    pMMI->ptMinTrackSize.x = 664;
    pMMI->ptMinTrackSize.y = 200;
}

/*
 * When the window resizes, we have to tell the edit box to expand, and
 * rearrange the controls inside it.
 */
void
ViewFilesDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);

    //WMSG2("Dialog: old size %d,%d\n",
    //  fLastWinSize.Width(), fLastWinSize.Height());
    WMSG2("Dialog: new size %d,%d\n", cx, cy);

    if (fLastWinSize.Width() == cx && fLastWinSize.Height() == cy) {
        WMSG0("VFD OnSize: no change\n");
        return;
    }

    int deltaX, deltaY;
    deltaX = cx - fLastWinSize.Width();
    deltaY = cy - fLastWinSize.Height();
    //WMSG2("Delta is %d,%d\n", deltaX, deltaY);

    ShiftControls(deltaX, deltaY);

    GetClientRect(&fLastWinSize);
}

/*
 * Adjust the positions and sizes of the controls.
 *
 * This relies on MinMaxInfo to guarantee that nothing falls off an edge.
 */
void
ViewFilesDialog::ShiftControls(int deltaX, int deltaY)
{
    HDWP hdwp;

    /*
     * Use deferred reposn so that they don't end up drawing on top of each
     * other and getting all weird.
     *
     * IMPORTANT: the DeferWindowPos stuff changes the tab order of the
     * items in the window.  The controls must be added in the reverse
     * order in which they appear in the window.
     */
    hdwp = BeginDeferWindowPos(15);
    hdwp = MoveControl(hdwp, this, AFX_IDW_SIZE_BOX, deltaX, deltaY);
    hdwp = MoveControl(hdwp, this, IDHELP, deltaX, deltaY);
    hdwp = MoveControl(hdwp, this, IDC_FVIEW_FONT, deltaX, deltaY);
    hdwp = MoveControl(hdwp, this, IDC_FVIEW_PRINT, deltaX, deltaY);
    hdwp = MoveControl(hdwp, this, IDC_FVIEW_FIND, deltaX, deltaY);
    hdwp = MoveControl(hdwp, this, IDC_FVIEW_FMT_RAW, 0, deltaY);
    hdwp = MoveControl(hdwp, this, IDC_FVIEW_FMT_HEX, 0, deltaY);
    hdwp = MoveControl(hdwp, this, IDC_FVIEW_FMT_BEST, 0, deltaY);
    hdwp = MoveControl(hdwp, this, IDC_FVIEW_PREV, 0, deltaY);
    hdwp = MoveControl(hdwp, this, IDC_FVIEW_NEXT, 0, deltaY);
    hdwp = MoveControl(hdwp, this, IDC_FVIEW_CMMT, 0, deltaY);
    hdwp = MoveControl(hdwp, this, IDC_FVIEW_RSRC, 0, deltaY);
    hdwp = MoveControl(hdwp, this, IDC_FVIEW_DATA, 0, deltaY);
    hdwp = MoveStretchControl(hdwp, this, IDC_FVIEW_FORMATSEL, 0, deltaY, deltaX, 0);
    hdwp = StretchControl(hdwp, this, IDC_FVIEW_EDITBOX, deltaX, deltaY);
    hdwp = MoveControl(hdwp, this, IDOK, deltaX, deltaY);
    if (!EndDeferWindowPos(hdwp)) {
        WMSG0("EndDeferWindowPos failed\n");
    }

    /*
     * Work around buggy CRichEdit controls.  The inner edit area is only
     * resized when the box is shrunk, not when it's expanded, and the
     * results are inconsistent between Win98 and Win2K.
     *
     * Set the internal size equal to the size of the entire edit box.
     * This should be large enough to make everything work right, but small
     * enough to avoid funky scrolling behavior.  (If you want to set this
     * more precisely, don't forget that scroll bars are not part of the
     * edit control client area, and their sizes must be factored in.)
     */
    CRect rect;
    CRichEditCtrl* pEdit = (CRichEditCtrl*) GetDlgItem(IDC_FVIEW_EDITBOX);
    ASSERT(pEdit != nil);
    //pEdit->GetClientRect(&rect);
    pEdit->GetWindowRect(&rect);
    //GetClientRect(&rect);
    rect.left = 2;
    rect.top = 2;
    pEdit->SetRect(&rect);
}


/*
 * Shuffle data in and out.
 */
void
ViewFilesDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    if (pDX->m_bSaveAndValidate) {
        WMSG0("COPY OUT\n");
    } else {
        WMSG0("COPY IN\n");
        OnFviewNext();
    }
}

static void
DumpBitmapInfo(HBITMAP hBitmap)
{
    BITMAP info;
    CBitmap* pBitmap = CBitmap::FromHandle(hBitmap);
    int gotten;

    gotten = pBitmap->GetObject(sizeof(info), &info);

    WMSG2("DumpBitmapInfo: gotten=%d of %d\n", gotten, sizeof(info));
    WMSG1("  bmType = %d\n", info.bmType);
    WMSG2("  bmWidth=%d, bmHeight=%d\n", info.bmWidth, info.bmHeight);
    WMSG1("  bmWidthBytes=%d\n", info.bmWidthBytes);
    WMSG1("  bmPlanes=%d\n", info.bmPlanes);
    WMSG1("  bmBitsPixel=%d\n", info.bmBitsPixel);
    WMSG1("  bmPits = 0x%08lx\n", info.bmBits);

}

/*
 * Display a buffer of text in the RichEdit control.
 *
 * The RichEdit dialog will hold its own copy of the data, so "pHolder" can
 * be safely destroyed after this returns.
 *
 * "fileName" is for display only.
 */
void
ViewFilesDialog::DisplayText(const WCHAR* fileName)
{
    CWaitCursor wait;   // streaming of big files can take a little while
    bool errFlg;
    bool emptyFlg = false;
    bool editHadFocus = false;
    
    ASSERT(fpOutput != nil);
    ASSERT(fileName != nil);

    errFlg = fpOutput->GetOutputKind() == ReformatOutput::kOutputErrorMsg;

    ASSERT(fpOutput->GetOutputKind() != ReformatOutput::kOutputUnknown);

    CRichEditCtrl* pEdit = (CRichEditCtrl*) GetDlgItem(IDC_FVIEW_EDITBOX);
    ASSERT(pEdit != nil);

    /* retain the selection even if we lose focus [can't do this in OnInit] */
    pEdit->SetOptions(ECOOP_OR, ECO_SAVESEL);

#if 0
    /*
     * Start by trashing anything that's there.  Not strictly necessary,
     * but it prevents the control from trying to maintain the old stuff
     * in an undo buffer.  (Not entirely sure if a stream-in operation is
     * undoable, but it costs very little to be certain.)
     *
     * UPDATE: I turned this off because it was dinging the speaker (?!).
     * Might be doing that because it's in read-only mode.
     */
    pEdit->SetSel(0, -1);
    pEdit->Clear();
    pEdit->EmptyUndoBuffer();
#endif

    /*
     * There's a redraw flash that goes away if you change the input
     * focus to something other than the edit ctrl.  (Move between large
     * files; it looks like you can see the text being selected and
     * hightlighted.  The control doesn't have an "always highlight" flag
     * set, so if the focus is on a different control it doesn't light up.)
     *
     * Since we're currently forcing the focus to be on the edit ctrl later
     * on, we just jam it to something else here.  If nothing has the focus,
     * as can happen if we click on "resource fork" and then Alt-N to a
     * file without a resource fork, we force the focus to return to the
     * edit window.
     *
     * NOTE: this would look a little better if we used the Prev/Next
     * buttons to hold the temporary focus, but we need to know which key
     * the user hit.  We could also create a bogus control, move it into
     * negative space where it will be invisible, and use that as a "focus
     * holder".
     */
    CWnd* pFocusWnd = GetFocus();
    if (pFocusWnd == nil || pFocusWnd->m_hWnd == pEdit->m_hWnd) {
        editHadFocus = true;
        GetDlgItem(IDOK)->SetFocus();
    }

    /*
     * The text color isn't getting reset when we reload the control.  I
     * can't find a "set default text color" call, so I'm reformatting
     * part of the buffer.
     *
     * Here's the weird part: it doesn't seem to matter what color I
     * set it to under Win2K.  It reverts to black so long as I do anything
     * here.  Under Win98, it uses the new color.
     */
    //if (0)
    {
        CHARFORMAT cf;
        cf.cbSize = sizeof(CHARFORMAT);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = RGB(0, 0, 0);
        pEdit->SetSel(0, 1);    // must select at least one char
        pEdit->SetSelectionCharFormat(cf);
    }

    /*
     * Add the appropriate data.  If the "bitmap" flag is set, use the
     * MyDIBitmap pointer instead.
     */
    if (fpOutput->GetOutputKind() == ReformatOutput::kOutputBitmap) {
        CClientDC dcScreen(this);
        HBITMAP hBitmap;

        if (fpRichEditOle == nil) {
            /* can't do this in OnInitDialog -- m_pWnd isn't initialized */
            fpRichEditOle = pEdit->GetIRichEditOle();
            ASSERT(fpRichEditOle != nil);
        }

        //FILE* fp = fopen("C:/test/output.bmp", "wb");
        //if (fp != nil) {
        //  pDib->WriteToFile(fp);
        //  fclose(fp);
        //}
        
        hBitmap = fpOutput->GetDIB()->ConvertToDDB(dcScreen.m_hDC);
        if (hBitmap == nil) {
            WMSG0("ConvertToDDB failed!\n");
            pEdit->SetWindowText(L"Internal error.");
            errFlg = true;
        } else {
            //DumpBitmapInfo(hBitmap);
            //DumpBitmapInfo(pDib->GetHandle());

            WMSG0("Inserting bitmap\n");
            pEdit->SetWindowText(L"");
            CImageDataObject::InsertBitmap(fpRichEditOle, hBitmap);

            /* RichEditCtrl has it now */
            ::DeleteObject(hBitmap);
        }
    } else {
        /*
         * Stream the data in, using the appropriate format.  Since we don't
         * have the "replace selection" flag set, this replaces everything
         * that's currently in there.
         *
         * We can't use SetWindowText() unless we're willing to forgo viewing
         * of binary files in "raw" form.  There doesn't seem to be any other
         * difference between the two approaches.
         */
        const char* textBuf;
        long textLen;
        int streamFormat;

        textBuf = fpOutput->GetTextBuf();
        textLen = fpOutput->GetTextLen();
        streamFormat = SF_TEXT;
        if (fpOutput->GetOutputKind() == ReformatOutput::kOutputRTF)
            streamFormat = SF_RTF;
        if (fpOutput->GetTextLen() == 0) {
            textBuf = "(file is empty)";
            textLen = strlen(textBuf);
            emptyFlg = true;
            EnableFormatSelection(FALSE);
        }
        if (fpOutput->GetOutputKind() == ReformatOutput::kOutputErrorMsg)
            EnableFormatSelection(FALSE);

        /* make sure the control will hold everything we throw at it */
        pEdit->LimitText(textLen+1);
        WMSG2("Streaming %ld bytes (kind=%d)\n",
            textLen, fpOutput->GetOutputKind());

        /* clear this early to avoid loading onto yellow */
        if (errFlg)
            pEdit->SetBackgroundColor(FALSE, RGB(255, 255, 0));
        else if (emptyFlg)
            pEdit->SetBackgroundColor(FALSE, RGB(192, 192, 192));
        else
            pEdit->SetBackgroundColor(TRUE, 0);

        RichEditXfer xfer(textBuf, textLen);
        EDITSTREAM es;
        es.dwCookie = (DWORD) &xfer;
        es.dwError = 0;
        es.pfnCallback = RichEditXfer::EditStreamCallback;
        long count;
        count = pEdit->StreamIn(streamFormat, es);
        WMSG2("StreamIn returned count=%ld dwError=%d\n", count, es.dwError);

        if (es.dwError != 0) {
            /* a -16 error can happen if the type is RTF but contents are not */
            char errorText[256];

            sprintf(errorText,
                "ERROR: failed while loading data (err=0x%08lx)\n"
                "(File contents might be too big for Windows to display)\n",
                es.dwError);
            RichEditXfer errXfer(errorText, strlen(errorText));
            es.dwCookie = (DWORD) &errXfer;
            es.dwError = 0;

            count = pEdit->StreamIn(SF_TEXT, es);
            WMSG2("Error StreamIn returned count=%ld dwError=%d\n", count, es.dwError);

            errFlg = true;
        }

        //pEdit->SetSel(0, 0);
    }

    /* move us back to the top */
    pEdit->LineScroll(-pEdit->GetFirstVisibleLine());

    /* just in case it's trying to hold on to something */
    pEdit->EmptyUndoBuffer();

    /* work around bug that creates unnecessary scroll bars */
    pEdit->SetScrollRange(SB_VERT, 0, 0, TRUE);
    pEdit->SetScrollRange(SB_HORZ, 0, 0, TRUE);

    /* display the entire message in the user's selected font */
    if (!fpOutput->GetMultipleFontsFlag()) {
        // adjust the font, stripping default boldness from SF_TEXT
        NewFontSelected(fpOutput->GetOutputKind() != ReformatOutput::kOutputRTF);
    }

    /* enable/disable the scroll bars */
    //pEdit->EnableScrollBar(SB_BOTH, ESB_DISABLE_BOTH);

    if (errFlg)
        pEdit->SetBackgroundColor(FALSE, RGB(255, 255, 0));
    else if (emptyFlg)
        pEdit->SetBackgroundColor(FALSE, RGB(192, 192, 192));
    else
        pEdit->SetBackgroundColor(TRUE, 0);

    /*
     * Work around a Windows bug that prevents the scroll bars from
     * being displayed immediately.  This makes them appear, but the
     * vertical scroll bar comes out funky on short files (fixed with
     * the SetScrollRange call above).
     *
     * Best guess: when the edit box is resized, it chooses the scroll bar
     * configuration based on the currently-loaded data.  If you resize it
     * and *then* add data, you're stuck with the previous scroll bar
     * values.  This doesn't quite make sense though...
     *
     * This works:
     *  - Set up dialog.
     *  - Load data.
     *  - Do minor twiddling.
     *  - Resize box significantly.
     *
     * This works:
     *  - (box already has data in it)
     *  - Load new data.
     *  - Do minor twiddling.
     *
     * This doesn't:
     *  - Set up dialog
     *  - Resize box significantly.
     *  - Load data.
     *  - Do minor twiddling.
     *
     * There might be some first-time setup issues in here.  Hard to say.
     * Anything related to RichEdit controls is extremely fragile, and must
     * be tested with a variety of inputs, preference settings, and under
     * at least Win98 and Win2K (which are *very* different).
     */
    if (fFirstResize) {
        /* adjust the size of the window to match the last size used */
        const Preferences* pPreferences = GET_PREFERENCES();
        long width = pPreferences->GetPrefLong(kPrFileViewerWidth);
        long height = pPreferences->GetPrefLong(kPrFileViewerHeight);
        CRect fullRect;
        GetWindowRect(&fullRect);
        //WMSG2(" VFD pre-size %dx%d\n", fullRect.Width(), fullRect.Height());
        fullRect.right = fullRect.left + width;
        fullRect.bottom = fullRect.top + height;
        MoveWindow(fullRect, TRUE);

        editHadFocus = true;    // force focus on edit box

        fFirstResize = false;
    } else {
        /* this should be enough */
        ShiftControls(0, 0);
    }
        
    if (fpOutput->GetOutputKind() == ReformatOutput::kOutputBitmap) {
        /* get the cursor off of the image */
        pEdit->SetSel(-1, -1);
    }

    /*
     * We want the focus to be on the text window so keyboard selection
     * commands work.  However, it's also nice to be able to arrow through
     * the format selection box.
     */
    if (editHadFocus)
        pEdit->SetFocus();

    fTitle = fileName;
    //if (fpOutput->GetOutputKind() == ReformatOutput::kOutputText ||
    //  fpOutput->GetOutputKind() == ReformatOutput::kOutputRTF ||
    //  fpOutput->GetOutputKind() == ReformatOutput::kOutputCSV ||
    //  fpOutput->GetOutputKind() == ReformatOutput::kOutputBitmap ||
    //  fpOutput->GetOutputKind() == ReformatOutput::kOutputRaw)
    //{
        // not for error messages
        fTitle += _T(" [");
        fTitle += fpOutput->GetFormatDescr();
        fTitle += _T("]");
    //} else if (fpOutput->GetOutputKind() == ReformatOutput::kOutputRaw) {
    //  fTitle += _T(" [Raw]");
    //}

    CString winTitle = _T("File Viewer - ");
    winTitle += fTitle;
    SetWindowText(winTitle);

    /*
     * Enable or disable the next/prev buttons.
     */
    CButton* pButton;
    pButton = (CButton*) GetDlgItem(IDC_FVIEW_PREV);
    pButton->EnableWindow(fpSelSet->IterHasPrev());
    pButton = (CButton*) GetDlgItem(IDC_FVIEW_NEXT);
    pButton->EnableWindow(fpSelSet->IterHasNext());
}

/*
 * Handle the "next" button.
 *
 * (This is also called from DoDataExchange.)
 */
void
ViewFilesDialog::OnFviewNext(void)
{
    ReformatHolder::ReformatPart part;
    ReformatHolder::ReformatID id;
    int result;

    if (fBusy) {
        WMSG0("BUSY!\n");
        return;
    }

    fBusy = true;

    if (!fpSelSet->IterHasNext()) {
        ASSERT(false);
        return;
    }

    /*
     * Get the pieces of the file.
     */
    SelectionEntry* pSelEntry = fpSelSet->IterNext();
    GenericEntry* pEntry = pSelEntry->GetEntry();
    result = ReformatPrep(pEntry);
#if 0
    {
        // for debugging -- simulate failure
        result = -1;
        delete fpHolder;
        fpHolder = nil;
        delete fpOutput;
        fpOutput = nil;
    }
#endif

    fBusy = false;
    if (result != 0) {
        ASSERT(fpHolder == nil);
        ASSERT(fpOutput == nil);
        return;
    }

    /*
     * Format a piece.
     */
    ConfigurePartButtons(pSelEntry->GetEntry());
    part = GetSelectedPart();
    id = ConfigureFormatSel(part);
    Reformat(pSelEntry->GetEntry(), part, id);

    DisplayText(pSelEntry->GetEntry()->GetDisplayName());
}

/*
 * Handle the "prev" button.
 */
void
ViewFilesDialog::OnFviewPrev(void)
{
    ReformatHolder::ReformatPart part;
    ReformatHolder::ReformatID id;
    int result;

    if (fBusy) {
        WMSG0("BUSY!\n");
        return;
    }

    fBusy = true;

    if (!fpSelSet->IterHasPrev()) {
        ASSERT(false);
        return;
    }

    /*
     * Get the pieces of the file.
     */
    SelectionEntry* pSelEntry = fpSelSet->IterPrev();
    GenericEntry* pEntry = pSelEntry->GetEntry();
    result = ReformatPrep(pEntry);

    fBusy = false;
    if (result != 0) {
        ASSERT(fpHolder == nil);
        ASSERT(fpOutput == nil);
        return;
    }

    /*
     * Format a piece.
     */
    ConfigurePartButtons(pEntry);
    part = GetSelectedPart();
    id = ConfigureFormatSel(part);
    Reformat(pEntry, part, id);

    DisplayText(pEntry->GetDisplayName());
}

/*
 * Configure the radio buttons that determine which part to view, enabling
 * only those that make sense.
 *
 * Try to keep the previously-set button set.
 *
 * If "pEntry" is nil, all buttons are disabled (useful for first-time
 * initialization).
 */
void
ViewFilesDialog::ConfigurePartButtons(const GenericEntry* pEntry)
{
    int id = 0;

    CButton* pDataWnd = (CButton*) GetDlgItem(IDC_FVIEW_DATA);
    CButton* pRsrcWnd = (CButton*) GetDlgItem(IDC_FVIEW_RSRC);
    CButton* pCmmtWnd = (CButton*) GetDlgItem(IDC_FVIEW_CMMT);
    ASSERT(pDataWnd != nil && pRsrcWnd != nil && pCmmtWnd != nil);

    /* figure out what was checked before, ignoring if it's not available */
    if (pDataWnd->GetCheck() == BST_CHECKED && pEntry->GetHasDataFork())
        id = IDC_FVIEW_DATA;
    else if (pRsrcWnd->GetCheck() == BST_CHECKED && pEntry->GetHasRsrcFork())
        id = IDC_FVIEW_RSRC;
    else if (pCmmtWnd->GetCheck() == BST_CHECKED && pEntry->GetHasComment())
        id = IDC_FVIEW_CMMT;

    /* couldn't keep previous check, find a new one */
    if (id == 0) {
        if (pEntry->GetHasDataFork())
            id = IDC_FVIEW_DATA;
        else if (pEntry->GetHasRsrcFork())
            id = IDC_FVIEW_RSRC;
        else if (pEntry->GetHasComment())
            id = IDC_FVIEW_CMMT;
        // else leave it set to 0
    }

    /* set up the dialog */
    pDataWnd->SetCheck(BST_UNCHECKED);
    pRsrcWnd->SetCheck(BST_UNCHECKED);
    pCmmtWnd->SetCheck(BST_UNCHECKED);

    if (pEntry == nil) {
        pDataWnd->EnableWindow(FALSE);
        pRsrcWnd->EnableWindow(FALSE);
        pCmmtWnd->EnableWindow(FALSE);
    } else {
        pDataWnd->EnableWindow(pEntry->GetHasDataFork());
        pRsrcWnd->EnableWindow(pEntry->GetHasRsrcFork());
        pCmmtWnd->EnableWindow(pEntry->GetHasComment());
    }

    if (id == IDC_FVIEW_RSRC)
        pRsrcWnd->SetCheck(BST_CHECKED);
    else if (id == IDC_FVIEW_CMMT)
        pCmmtWnd->SetCheck(BST_CHECKED);
    else
        pDataWnd->SetCheck(BST_CHECKED);
}

/*
 * Figure out which part of the file is selected (data/rsrc/comment).
 *
 * If no part is selected, throws up its hands and returns kPartData.
 */
ReformatHolder::ReformatPart
ViewFilesDialog::GetSelectedPart(void)
{
    CButton* pDataWnd = (CButton*) GetDlgItem(IDC_FVIEW_DATA);
    CButton* pRsrcWnd = (CButton*) GetDlgItem(IDC_FVIEW_RSRC);
    CButton* pCmmtWnd = (CButton*) GetDlgItem(IDC_FVIEW_CMMT);
    ASSERT(pDataWnd != nil && pRsrcWnd != nil && pCmmtWnd != nil);

    if (pDataWnd->GetCheck() == BST_CHECKED)
        return ReformatHolder::kPartData;
    else if (pRsrcWnd->GetCheck() == BST_CHECKED)
        return ReformatHolder::kPartRsrc;
    else if (pCmmtWnd->GetCheck() == BST_CHECKED)
        return ReformatHolder::kPartCmmt;
    else {
        assert(false);
        return ReformatHolder::kPartData;
    }
}

/*
 * Set up the fpHolder.  Does not reformat the data, just loads the source
 * material and runs the applicability tests.
 *
 * Returns 0 on success, -1 on failure.
 */
int
ViewFilesDialog::ReformatPrep(GenericEntry* pEntry)
{
    CWaitCursor waitc;      // can be slow reading data from floppy
    MainWindow* pMainWindow = GET_MAIN_WINDOW();
    int result;

    delete fpHolder;
    fpHolder = nil;

    result = pMainWindow->GetFileParts(pEntry, &fpHolder);
    if (result != 0) {
        WMSG0("GetFileParts(prev) failed!\n");
        ASSERT(fpHolder == nil);
        return -1;
    }

    /* set up the ReformatHolder */
    MainWindow::ConfigureReformatFromPreferences(fpHolder);

    /* prep for applicability test */
    fpHolder->SetSourceAttributes(
        pEntry->GetFileType(),
        pEntry->GetAuxType(),
        MainWindow::ReformatterSourceFormat(pEntry->GetSourceFS()),
        pEntry->GetFileNameExtensionA());

    /* figure out which reformatters apply to this file */
    WMSG0("Testing reformatters\n");
    fpHolder->TestApplicability();

    return 0;
}

/*
 * Reformat a file.
 *
 * Returns 0 if the file was reformatted, -1 if not
 */
int
ViewFilesDialog::Reformat(const GenericEntry* pEntry,
    ReformatHolder::ReformatPart part, ReformatHolder::ReformatID id)
{
    CWaitCursor waitc;

    delete fpOutput;
    fpOutput = nil;

    /* run the best one */
    fpOutput = fpHolder->Apply(part, id);

//bail:
    if (fpOutput != nil) {
        // success -- do some sanity checks
        switch (fpOutput->GetOutputKind()) {
        case ReformatOutput::kOutputText:
        case ReformatOutput::kOutputRTF:
        case ReformatOutput::kOutputCSV:
        case ReformatOutput::kOutputErrorMsg:
            ASSERT(fpOutput->GetTextBuf() != nil);
            ASSERT(fpOutput->GetDIB() == nil);
            break;
        case ReformatOutput::kOutputBitmap:
            ASSERT(fpOutput->GetDIB() != nil);
            ASSERT(fpOutput->GetTextBuf() == nil);
            break;
        case ReformatOutput::kOutputRaw:
            // text buf might be nil
            ASSERT(fpOutput->GetDIB() == nil);
            break;
        }
        return 0;
    } else {
        /* shouldn't get here; handle it if we do */
        static const char* kFailMsg = "Internal error\r\n";
        fpOutput = new ReformatOutput;
        fpOutput->SetTextBuf((char*) kFailMsg, strlen(kFailMsg), false);
        fpOutput->SetOutputKind(ReformatOutput::kOutputErrorMsg);
        return -1;
    }
}

/*
 * Set up the entries in the drop box based on the "applicable" array in
 * fpHolder.  The set of values is different for each part of the file.
 *
 * Returns the default reformatter ID.  This is always entry #0.
 *
 * I tried making it "sticky", so that if the user chose to go into hex
 * dump mode it would stay there.  We returned either entry #0 in the
 * combo box, or the previously-selected reformatter ID if it also
 * applies to this file.  This was a little whacked, e.g. Intbasic vs.
 * S-C assembler got ugly, so I tried restricting it to "always" classes.
 * But then, the first time you hit a binary file with no reformatter,
 * you're stuck there.  Eventually I decided to discard the idea.
 */
ReformatHolder::ReformatID
ViewFilesDialog::ConfigureFormatSel(ReformatHolder::ReformatPart part)
{
    //ReformatHolder::ReformatID prevID = ReformatHolder::kReformatUnknown;
    ReformatHolder::ReformatID returnID = ReformatHolder::kReformatRaw;
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_FVIEW_FORMATSEL);

    WMSG0("--- ConfigureFormatSel\n");

    //int sel;
    //sel = pCombo->GetCurSel();
    //if (sel != CB_ERR)
    //  prevID = (ReformatHolder::ReformatID) pCombo->GetItemData(sel);
    //WMSG1("  prevID = %d\n", prevID);

    EnableFormatSelection(TRUE);
    pCombo->ResetContent();

    /*
     * Fill out the combo box with the reformatter entries.
     *
     * There's probably a way to do this that doesn't involve abusing
     * enums, but this'll do for now.
     */
    int applyIdx, idIdx;
    bool preferred = true;
    int comboIdx;
    for (applyIdx = ReformatHolder::kApplicMAX;
        applyIdx > ReformatHolder::kApplicNot; /*no incr*/)
    {
        if (applyIdx == ReformatHolder::kApplicMAX)
            goto skip;
        if (applyIdx == ReformatHolder::kApplicUnknown)
            goto skip;

        int testApplies;

        testApplies = applyIdx;
        if (preferred)
            testApplies |= ReformatHolder::kApplicPreferred;

        for (idIdx = 0; idIdx < ReformatHolder::kReformatMAX; idIdx++) {
            if (idIdx == ReformatHolder::kReformatUnknown)
                continue;

            ReformatHolder::ReformatApplies applies;

            applies = fpHolder->GetApplic(part,
                                        (ReformatHolder::ReformatID) idIdx);
            if ((int) applies == testApplies) {
                /* match! */
                CString str;
                //WMSG2("MATCH at %d (0x%02x)\n", idIdx, testApplies);
                str.Format(L"%ls", ReformatHolder::GetReformatName(
                                        (ReformatHolder::ReformatID) idIdx));
                comboIdx = pCombo->AddString(str);
                pCombo->SetItemData(comboIdx, idIdx);

                /* define initial selection as best item */
                if (comboIdx == 0)
                    pCombo->SetCurSel(comboIdx);

                //if (idIdx == (int) prevID &&
                //  applyIdx == ReformatHolder::kApplicAlways)
                //{
                //  WMSG0("  Found 'always' prevID, selecting\n");
                //  pCombo->SetCurSel(comboIdx);
                //}
            }
        }

skip:
        if (!preferred)
            applyIdx--;
        preferred = !preferred;
    }

    /* return whatever we now have selected */
    int sel = pCombo->GetCurSel();
    WMSG1("  At end, sel is %d\n", sel);
    if (sel != CB_ERR)
        returnID = (ReformatHolder::ReformatID) pCombo->GetItemData(sel);

    return returnID;
}

/*
 * The user has changed entries in the format selection drop box.
 *
 * Also called from the "quick change" buttons.
 */
void
ViewFilesDialog::OnFormatSelChange(void)
{
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_FVIEW_FORMATSEL);
    ASSERT(pCombo != nil);
    WMSG1("+++ SELECTION IS NOW %d\n", pCombo->GetCurSel());

    SelectionEntry* pSelEntry = fpSelSet->IterCurrent();
    GenericEntry* pEntry = pSelEntry->GetEntry();
    ReformatHolder::ReformatPart part;
    ReformatHolder::ReformatID id;

    part = GetSelectedPart();
    id = (ReformatHolder::ReformatID) pCombo->GetItemData(pCombo->GetCurSel());
    Reformat(pEntry, part, id);

    DisplayText(pEntry->GetDisplayName());
}

/*
 * Change the fork we're looking at.
 */
void
ViewFilesDialog::OnFviewData(void)
{
    ForkSelectCommon(ReformatHolder::kPartData);
}
void
ViewFilesDialog::OnFviewRsrc(void)
{
    ForkSelectCommon(ReformatHolder::kPartRsrc);
}
void
ViewFilesDialog::OnFviewCmmt(void)
{
    ForkSelectCommon(ReformatHolder::kPartCmmt);
}
void
ViewFilesDialog::ForkSelectCommon(ReformatHolder::ReformatPart part)
{
    GenericEntry* pEntry;
    ReformatHolder::ReformatID id;

    WMSG1("Switching to file part=%d\n", part);
    ASSERT(fpHolder != nil);
    ASSERT(fpSelSet != nil);
    ASSERT(fpSelSet->IterCurrent() != nil);
    pEntry = fpSelSet->IterCurrent()->GetEntry();
    ASSERT(pEntry != nil);

    id = ConfigureFormatSel(part);

    Reformat(pEntry, part, id);
    DisplayText(pEntry->GetDisplayName());
}

/*
 * Switch to hex dump mode.
 */
void
ViewFilesDialog::OnFviewFmtBest(void)
{
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_FVIEW_FORMATSEL);
    pCombo->SetCurSel(0);       // best is always at the top
    OnFormatSelChange();
    pCombo->SetFocus();
}

/*
 * Switch to hex dump mode.
 */
void
ViewFilesDialog::OnFviewFmtHex(void)
{
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_FVIEW_FORMATSEL);
    int sel = FindByVal(pCombo, ReformatHolder::kReformatHexDump);
    if (sel < 0)
        sel = 0;
    pCombo->SetCurSel(sel);
    OnFormatSelChange();
    pCombo->SetFocus();
}

/*
 * Switch to raw mode.
 */
void
ViewFilesDialog::OnFviewFmtRaw(void)
{
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_FVIEW_FORMATSEL);
    int sel = FindByVal(pCombo, ReformatHolder::kReformatRaw);
    if (sel < 0)
        sel = 0;
    pCombo->SetCurSel(sel);
    OnFormatSelChange();
    pCombo->SetFocus();
}

/*
 * Enable or disable all of the format selection buttons.
 */
void
ViewFilesDialog::EnableFormatSelection(BOOL enable)
{
    GetDlgItem(IDC_FVIEW_FORMATSEL)->EnableWindow(enable);
    GetDlgItem(IDC_FVIEW_FMT_BEST)->EnableWindow(enable);
    GetDlgItem(IDC_FVIEW_FMT_HEX)->EnableWindow(enable);
    GetDlgItem(IDC_FVIEW_FMT_RAW)->EnableWindow(enable);
}

/*
 * Return the combo box index for the entry whose "data" field matches "val".
 *
 * Returns -1 if the entry couldn't be found.
 */
int
ViewFilesDialog::FindByVal(CComboBox* pCombo, DWORD val)
{
    int count = pCombo->GetCount();
    int i;

    for (i = 0; i < count; i++) {
        if (pCombo->GetItemData(i) == val)
            return i;
    }
    return -1;
}

/*
 * Handle the "font" button.  Choose a font, then apply the choice to
 * all of the text in the box.
 */
void
ViewFilesDialog::OnFviewFont(void)
{
    LOGFONT logFont;
    CFont font;

    /*
     * Create a LOGFONT structure with the desired default characteristics,
     * then use that to initialize the font dialog.
     */
    CreateSimpleFont(&font, this, fTypeFace, fPointSize);
    font.GetLogFont(&logFont);

    CFontDialog fontDlg(&logFont);
    fontDlg.m_cf.Flags &= ~(CF_EFFECTS);

    if (fontDlg.DoModal() == IDOK) {
        //fontDlg.GetCurrentFont(&logFont);
        fTypeFace = fontDlg.GetFaceName();
        fPointSize = fontDlg.GetSize() / 10;
        WMSG2("Now using %d-point '%ls'\n", fPointSize, (LPCWSTR) fTypeFace);

        NewFontSelected(false);
    }

}

/*
 * Font selection has changed, update the richedit box.
 */
void
ViewFilesDialog::NewFontSelected(bool resetBold)
{
    CRichEditCtrl* pEdit = (CRichEditCtrl*) GetDlgItem(IDC_FVIEW_EDITBOX);
    ASSERT(pEdit != nil);

    CHARFORMAT cf;
    cf.cbSize = sizeof(CHARFORMAT);
    cf.dwMask = CFM_FACE | CFM_SIZE;
    if (resetBold) {
        cf.dwMask |= CFM_BOLD;
        cf.dwEffects = 0;
    }
    ::lstrcpy(cf.szFaceName, fTypeFace);
    cf.yHeight = fPointSize * 20;       // in twips
    pEdit->SetSel(0, -1);               // select all
    pEdit->SetSelectionCharFormat(cf);
    pEdit->SetSel(0, 0);                // unselect
}


/*
 * Print a ContentList.
 */
void
ViewFilesDialog::OnFviewPrint(void)
{
    MainWindow* pMainWindow = GET_MAIN_WINDOW();
    CPrintDialog dlg(FALSE);    // use CPrintDialogEx for Win2K? CPageSetUpDialog?
    PrintRichEdit pre;
    CDC dc;
    int numPages;

    dlg.m_pd.nFromPage = dlg.m_pd.nMinPage = 1;
    dlg.m_pd.nToPage = dlg.m_pd.nMaxPage = 1;

    /*
     * Getting the expected number of pages requires a print test-run.
     * However, if we use GetDefaults to get the DC, the call to DoModal
     * returns immediately with IDCANCEL.  So, we do our pre-flighting
     * in a separate DC with a separate print dialog object.
     */
    {
        CPrintDialog countDlg(FALSE);
        CDC countDC;

        dlg.m_pd.hDevMode = pMainWindow->fhDevMode;
        dlg.m_pd.hDevNames = pMainWindow->fhDevNames;

        if (countDlg.GetDefaults() == TRUE) {
            CWaitCursor waitc;

            if (countDC.Attach(countDlg.GetPrinterDC()) != TRUE) {
                ASSERT(false);
            }
            pre.Setup(&countDC, this);
            pre.PrintPreflight(&fEditCtrl, &numPages);
            WMSG1("Default printer generated %d pages\n", numPages);

            dlg.m_pd.nToPage = dlg.m_pd.nMaxPage = numPages;
        }

        pMainWindow->fhDevMode = dlg.m_pd.hDevMode;
        pMainWindow->fhDevNames = dlg.m_pd.hDevNames;
    }

    long startChar, endChar;
    fEditCtrl.GetSel(/*ref*/startChar, /*ref*/endChar);

    if (endChar != startChar) {
        WMSG2("GetSel returned start=%ld end=%ld\n", startChar, endChar);
        dlg.m_pd.Flags &= ~(PD_NOSELECTION);
    }

    dlg.m_pd.hDevMode = pMainWindow->fhDevMode;
    dlg.m_pd.hDevNames = pMainWindow->fhDevNames;
    dlg.m_pd.Flags |= PD_USEDEVMODECOPIESANDCOLLATE;
    dlg.m_pd.Flags &= ~(PD_NOPAGENUMS);


    /*
     * Show them the print dialog.
     */
    if (dlg.DoModal() != IDOK)
        return;

    /*
     * Grab the chosen printer and prep ourselves.
     */
    if (dc.Attach(dlg.GetPrinterDC()) != TRUE) {
        CString msg;
        msg.LoadString(IDS_PRINTER_NOT_USABLE);
        ShowFailureMsg(this, msg, IDS_FAILED);
        return;
    }
    pre.Setup(&dc, this);

    /*
     * Do the printing.
     */
    if (dlg.PrintRange())
        pre.PrintPages(&fEditCtrl, fTitle, dlg.GetFromPage(), dlg.GetToPage());
    else if (dlg.PrintSelection())
        pre.PrintSelection(&fEditCtrl, fTitle, startChar, endChar);
    else    // dlg.PrintAll()
        pre.PrintAll(&fEditCtrl, fTitle);

    pMainWindow->fhDevMode = dlg.m_pd.hDevMode;
    pMainWindow->fhDevNames = dlg.m_pd.hDevNames;
}

/*
 * User hit the "Find..." button.  Open the modeless Find dialog.
 */
void
ViewFilesDialog::OnFviewFind(void)
{
    DWORD flags = 0;

    if (fpFindDialog != nil)
        return;

    if (fFindDown)
        flags |= FR_DOWN;
    if (fFindMatchCase)
        flags |= FR_MATCHCASE;
    if (fFindMatchWholeWord)
        flags |= FR_WHOLEWORD;

    /*
     * I can't get this to work with FindText().  There's a lot of questions
     * about this on web sites.  Probably safest to just disable it.
     */
    flags |= FR_HIDEUPDOWN;

    fpFindDialog = new CFindReplaceDialog;

    fpFindDialog->Create(TRUE,      // "find" only
                         fFindLastStr,  // default string to search for
                         NULL,      // default string to replace
                         flags,     // flags
                         this);     // parent
}

/*
 * Handle activity in the modeless "find" dialog.
 */
LRESULT
ViewFilesDialog::OnFindDialogMessage(WPARAM wParam, LPARAM lParam)
{
    assert(fpFindDialog != nil);

    fFindDown = (fpFindDialog->SearchDown() != 0);
    fFindMatchCase = (fpFindDialog->MatchCase() != 0);
    fFindMatchWholeWord = (fpFindDialog->MatchWholeWord() != 0);

    if (fpFindDialog->IsTerminating()) {
        WMSG0("VFD find dialog closing\n");
        fpFindDialog = nil;
        return 0;
    }

    if (fpFindDialog->FindNext()) {
        fFindLastStr = fpFindDialog->GetFindString();
        FindNext(fFindLastStr, fFindDown, fFindMatchCase, fFindMatchWholeWord);
    } else {
        WMSG0("Unexpected find dialog activity\n");
    }

    return 0;
}


/*
 * Find the next ocurrence of the specified string.
 */
void
ViewFilesDialog::FindNext(const WCHAR* str, bool down, bool matchCase,
    bool wholeWord)
{
    WMSG4("FindText '%ls' d=%d c=%d w=%d\n", str, down, matchCase, wholeWord);

    FINDTEXTEX findTextEx = { 0 };
    CHARRANGE selChrg;
    DWORD flags = 0;
    long start, result;

    if (matchCase)
        flags |= FR_MATCHCASE;
    if (wholeWord)
        flags |= FR_WHOLEWORD;

    fEditCtrl.GetSel(selChrg);
    WMSG2("  selection is %ld,%ld\n",
        selChrg.cpMin, selChrg.cpMax);
    if (selChrg.cpMin == selChrg.cpMax)
        start = selChrg.cpMin;      // start at caret
    else
        start = selChrg.cpMin +1;   // start past selection

    findTextEx.chrg.cpMin = start;
    findTextEx.chrg.cpMax = -1;
    findTextEx.lpstrText = str;

    /* MSVC++6 claims FindText doesn't exist, even though it's in the header */
    //result = fEditCtrl.FindText(flags, &findTextEx);
    result = fEditCtrl.SendMessage(EM_FINDTEXTEX, (WPARAM) flags,
        (LPARAM) &findTextEx);

    if (result == -1) {
        /* didn't find it, wrap around to start */
        findTextEx.chrg.cpMin = 0;
        findTextEx.chrg.cpMax = -1;
        findTextEx.lpstrText = str;
        result = fEditCtrl.SendMessage(EM_FINDTEXTEX, (WPARAM) flags,
            (LPARAM) &findTextEx);
    }

    WMSG3("  result=%ld min=%ld max=%ld\n", result,
        findTextEx.chrgText.cpMin, findTextEx.chrgText.cpMax);
    if (result != -1) {
        /* select the text we found */
        fEditCtrl.SetSel(findTextEx.chrgText);
    } else {
        /* remove selection, leaving caret at start of sel */
        selChrg.cpMax = selChrg.cpMin;
        fEditCtrl.SetSel(selChrg);

        MainWindow* pMain = (MainWindow*)::AfxGetMainWnd();
        pMain->FailureBeep();
    }
}

/*
 * User pressed the "Help" button.
 */
void
ViewFilesDialog::OnHelp(void)
{
    WinHelp(HELP_TOPIC_FILE_VIEWER, HELP_CONTEXT);
}
