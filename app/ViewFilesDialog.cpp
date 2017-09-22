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

BOOL ViewFilesDialog::OnInitDialog(void)
{
    LOGD("Now in VFD OnInitDialog!");

    ASSERT(fpSelSet != NULL);

    /* delete dummy control and insert our own with modded styles */
    CRichEditCtrl* pEdit = (CRichEditCtrl*)GetDlgItem(IDC_FVIEW_EDITBOX);
    ASSERT(pEdit != NULL);
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
    LOGI(" VFD pre-size %dx%d", fullRect.Width(), fullRect.Height());
    fullRect.right = fullRect.left + width;
    fullRect.bottom = fullRect.top + height;
    MoveWindow(fullRect, FALSE);
#endif

    // This invokes UpdateData, which calls DoDataExchange, which leads to
    // the StreamIn call.  So don't do this until everything else is ready.
    CDialog::OnInitDialog();

    LOGD("VFD OnInitDialog done");
    return FALSE;   // don't let Windows set the focus
}

int ViewFilesDialog::OnCreate(LPCREATESTRUCT lpcs)
{
    LOGD("VFD OnCreate");

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

    LOGD("VFD OnCreate done");
    return 0;
}

void ViewFilesDialog::OnDestroy(void)
{
    Preferences* pPreferences = GET_PREFERENCES_WR();
    CRect rect;
    GetWindowRect(&rect);

    pPreferences->SetPrefLong(kPrFileViewerWidth, rect.Width());
    pPreferences->SetPrefLong(kPrFileViewerHeight, rect.Height());

    CDialog::OnDestroy();
}

void ViewFilesDialog::OnOK(void)
{
    if (fBusy)
        MessageBeep(-1);
    else {
        CRect rect;

        GetWindowRect(&rect);
        LOGD(" VFD size now %dx%d", rect.Width(), rect.Height());

        CDialog::OnOK();
    }
}

void ViewFilesDialog::OnCancel(void)
{
    if (fBusy)
        MessageBeep(-1);
    else
        CDialog::OnCancel();
}

void ViewFilesDialog::OnGetMinMaxInfo(MINMAXINFO* pMMI)
{
    pMMI->ptMinTrackSize.x = 664;
    pMMI->ptMinTrackSize.y = 200;
}

void ViewFilesDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);

    //LOGI("Dialog: old size %d,%d",
    //  fLastWinSize.Width(), fLastWinSize.Height());
    LOGD("Dialog: new size %d,%d", cx, cy);

    if (fLastWinSize.Width() == cx && fLastWinSize.Height() == cy) {
        LOGD("VFD OnSize: no change");
        return;
    }

    int deltaX, deltaY;
    deltaX = cx - fLastWinSize.Width();
    deltaY = cy - fLastWinSize.Height();
    //LOGI("Delta is %d,%d", deltaX, deltaY);

    ShiftControls(deltaX, deltaY);

    GetClientRect(&fLastWinSize);
}

void ViewFilesDialog::ShiftControls(int deltaX, int deltaY)
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
        LOGI("EndDeferWindowPos failed");
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
    ASSERT(pEdit != NULL);
    //pEdit->GetClientRect(&rect);
    pEdit->GetWindowRect(&rect);
    //GetClientRect(&rect);
    rect.left = 2;
    rect.top = 2;
    pEdit->SetRect(&rect);
}


void ViewFilesDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    if (pDX->m_bSaveAndValidate) {
        LOGD("COPY OUT");
    } else {
        LOGD("COPY IN");
        OnFviewNext();
    }
}

static void DumpBitmapInfo(HBITMAP hBitmap)
{
    BITMAP info;
    CBitmap* pBitmap = CBitmap::FromHandle(hBitmap);
    int gotten;

    gotten = pBitmap->GetObject(sizeof(info), &info);

    LOGD("DumpBitmapInfo: gotten=%d of %d", gotten, sizeof(info));
    LOGD("  bmType = %d", info.bmType);
    LOGD("  bmWidth=%d, bmHeight=%d", info.bmWidth, info.bmHeight);
    LOGD("  bmWidthBytes=%d", info.bmWidthBytes);
    LOGD("  bmPlanes=%d", info.bmPlanes);
    LOGD("  bmBitsPixel=%d", info.bmBitsPixel);
    LOGD("  bmPits = 0x%p", info.bmBits);
}

bool ViewFilesDialog::IsSourceEmpty(const GenericEntry* pEntry,
    ReformatHolder::ReformatPart part)
{
    switch (part) {
    case ReformatHolder::ReformatPart::kPartData:
        return pEntry->GetDataForkLen() == 0;
    case ReformatHolder::ReformatPart::kPartRsrc:
        return pEntry->GetRsrcForkLen() == 0;
    case ReformatHolder::ReformatPart::kPartCmmt:
        return !pEntry->GetHasNonEmptyComment();
    default:
        return false;
    }
}

void ViewFilesDialog::DisplayText(const WCHAR* fileName, bool zeroSourceLen)
{
    CWaitCursor wait;   // streaming of big files can take a little while
    bool errFlg;
    bool emptyFlg = false;
    bool editHadFocus = false;
    
    ASSERT(fpOutput != NULL);
    ASSERT(fileName != NULL);

    errFlg = fpOutput->GetOutputKind() == ReformatOutput::kOutputErrorMsg;

    ASSERT(fpOutput->GetOutputKind() != ReformatOutput::kOutputUnknown);

    CRichEditCtrl* pEdit = (CRichEditCtrl*) GetDlgItem(IDC_FVIEW_EDITBOX);
    ASSERT(pEdit != NULL);

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
     *
     * TODO: on Win7 you can sometimes see a blue flash.  Not sure if it
     * relates to this or some other aspect of the redraw.
     */
    CWnd* pFocusWnd = GetFocus();
    if (pFocusWnd == NULL || pFocusWnd->m_hWnd == pEdit->m_hWnd) {
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

        if (fpRichEditOle == NULL) {
            /* can't do this in OnInitDialog -- m_pWnd isn't initialized */
            fpRichEditOle = pEdit->GetIRichEditOle();
            ASSERT(fpRichEditOle != NULL);
        }

        //FILE* fp = fopen("C:/test/output.bmp", "wb");
        //if (fp != NULL) {
        //  pDib->WriteToFile(fp);
        //  fclose(fp);
        //}
        
        hBitmap = fpOutput->GetDIB()->ConvertToDDB(dcScreen.m_hDC);
        if (hBitmap == NULL) {
            LOGW("ConvertToDDB failed!");
            pEdit->SetWindowText(L"Internal error.");
            errFlg = true;
        } else {
            //DumpBitmapInfo(hBitmap);
            //DumpBitmapInfo(pDib->GetHandle());

            LOGD("Inserting bitmap");
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
            if (zeroSourceLen) {
                textBuf = "(file is empty)";
                EnableFormatSelection(FALSE);
            } else {
                textBuf = "(converted output is empty)";
            }
            textLen = strlen(textBuf);
            emptyFlg = true;
        }
        if (fpOutput->GetOutputKind() == ReformatOutput::kOutputErrorMsg)
            EnableFormatSelection(FALSE);

        /* make sure the control will hold everything we throw at it */
        pEdit->LimitText(textLen+1);
        LOGI("Streaming %ld bytes (kind=%d)",
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
        LOGI("StreamIn returned count=%ld dwError=%d", count, es.dwError);

        if (es.dwError != 0) {
            /* a -16 error can happen if the type is RTF but contents are not */
            char errorText[256];

            _snprintf(errorText, sizeof(errorText),
                "ERROR: failed while loading data (err=0x%08lx)\n"
                "(File contents might be too big for Windows to display)\n",
                es.dwError);
            RichEditXfer errXfer(errorText, strlen(errorText));
            es.dwCookie = (DWORD) &errXfer;
            es.dwError = 0;

            count = pEdit->StreamIn(SF_TEXT, es);
            LOGI("Error StreamIn returned count=%ld dwError=%d", count, es.dwError);

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
     *
     * TODO: re-evaluate all this without worrying about Win9x
     */
    if (fFirstResize) {
        /* adjust the size of the window to match the last size used */
        const Preferences* pPreferences = GET_PREFERENCES();
        long width = pPreferences->GetPrefLong(kPrFileViewerWidth);
        long height = pPreferences->GetPrefLong(kPrFileViewerHeight);
        CRect fullRect;
        GetWindowRect(&fullRect);
        //LOGI(" VFD pre-size %dx%d", fullRect.Width(), fullRect.Height());
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

        /*
         * Tall Super Hi-Res graphics (e.g. Paintworks PNT) cause the edit
         * control to scroll to the bottom.  Move it back to the top.
         *
         * SetScrollInfo just moves the scrollbar without changing the
         * view position.
         */
        pEdit->SendMessage(WM_VSCROLL, SB_TOP, 0);
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

void ViewFilesDialog::OnFviewNext(void)
{
    // handles "next" button; also called from DoDataExchange
    ReformatHolder::ReformatPart part;
    ReformatHolder::ReformatID id;
    int result;

    if (fBusy) {
        LOGI("BUSY!");
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
        fpHolder = NULL;
        delete fpOutput;
        fpOutput = NULL;
    }
#endif

    fBusy = false;
    if (result != 0) {
        ASSERT(fpHolder == NULL);
        ASSERT(fpOutput == NULL);
        return;
    }

    /*
     * Format a piece.
     */
    ConfigurePartButtons(pSelEntry->GetEntry());
    part = GetSelectedPart();
    id = ConfigureFormatSel(part);
    Reformat(pSelEntry->GetEntry(), part, id);

    DisplayText(pSelEntry->GetEntry()->GetDisplayName(),
        IsSourceEmpty(pSelEntry->GetEntry(), part));
}

void ViewFilesDialog::OnFviewPrev(void)
{
    ReformatHolder::ReformatPart part;
    ReformatHolder::ReformatID id;
    int result;

    if (fBusy) {
        LOGI("BUSY!");
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
        ASSERT(fpHolder == NULL);
        ASSERT(fpOutput == NULL);
        return;
    }

    /*
     * Format a piece.
     */
    ConfigurePartButtons(pEntry);
    part = GetSelectedPart();
    id = ConfigureFormatSel(part);
    Reformat(pEntry, part, id);

    DisplayText(pEntry->GetDisplayName(), IsSourceEmpty(pEntry, part));
}

void ViewFilesDialog::ConfigurePartButtons(const GenericEntry* pEntry)
{
    int id = 0;

    CButton* pDataWnd = (CButton*) GetDlgItem(IDC_FVIEW_DATA);
    CButton* pRsrcWnd = (CButton*) GetDlgItem(IDC_FVIEW_RSRC);
    CButton* pCmmtWnd = (CButton*) GetDlgItem(IDC_FVIEW_CMMT);
    ASSERT(pDataWnd != NULL && pRsrcWnd != NULL && pCmmtWnd != NULL);

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

    if (pEntry == NULL) {
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

ReformatHolder::ReformatPart ViewFilesDialog::GetSelectedPart(void)
{
    CButton* pDataWnd = (CButton*) GetDlgItem(IDC_FVIEW_DATA);
    CButton* pRsrcWnd = (CButton*) GetDlgItem(IDC_FVIEW_RSRC);
    CButton* pCmmtWnd = (CButton*) GetDlgItem(IDC_FVIEW_CMMT);
    ASSERT(pDataWnd != NULL && pRsrcWnd != NULL && pCmmtWnd != NULL);

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

int ViewFilesDialog::ReformatPrep(GenericEntry* pEntry)
{
    CWaitCursor waitc;      // can be slow reading data from floppy
    MainWindow* pMainWindow = GET_MAIN_WINDOW();
    int result;

    delete fpHolder;
    fpHolder = NULL;

    result = pMainWindow->GetFileParts(pEntry, &fpHolder);
    if (result != 0) {
        LOGI("GetFileParts(prev) failed!");
        ASSERT(fpHolder == NULL);
        return -1;
    }

    /* set up the ReformatHolder */
    MainWindow::ConfigureReformatFromPreferences(fpHolder);

    /* prep for applicability test */
    fpHolder->SetSourceAttributes(
        pEntry->GetFileType(),
        pEntry->GetAuxType(),
        MainWindow::ReformatterSourceFormat(pEntry->GetSourceFS()),
        pEntry->GetFileNameExtensionMOR());

    /* figure out which reformatters apply to this file */
    LOGD("Testing reformatters");
    fpHolder->TestApplicability();

    return 0;
}

int ViewFilesDialog::Reformat(const GenericEntry* pEntry,
    ReformatHolder::ReformatPart part, ReformatHolder::ReformatID id)
{
    CWaitCursor waitc;

    delete fpOutput;
    fpOutput = NULL;

    /* run the best one */
    fpOutput = fpHolder->Apply(part, id);

//bail:
    if (fpOutput != NULL) {
        // success -- do some sanity checks
        switch (fpOutput->GetOutputKind()) {
        case ReformatOutput::kOutputText:
        case ReformatOutput::kOutputRTF:
        case ReformatOutput::kOutputCSV:
        case ReformatOutput::kOutputErrorMsg:
            ASSERT(fpOutput->GetTextBuf() != NULL);
            ASSERT(fpOutput->GetDIB() == NULL);
            break;
        case ReformatOutput::kOutputBitmap:
            ASSERT(fpOutput->GetDIB() != NULL);
            ASSERT(fpOutput->GetTextBuf() == NULL);
            break;
        case ReformatOutput::kOutputRaw:
            // text buf might be NULL
            ASSERT(fpOutput->GetDIB() == NULL);
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

ReformatHolder::ReformatID ViewFilesDialog::ConfigureFormatSel(
    ReformatHolder::ReformatPart part)
{
    //ReformatHolder::ReformatID prevID = ReformatHolder::kReformatUnknown;
    ReformatHolder::ReformatID returnID = ReformatHolder::kReformatRaw;
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_FVIEW_FORMATSEL);

    LOGD("--- ConfigureFormatSel");

    //int sel;
    //sel = pCombo->GetCurSel();
    //if (sel != CB_ERR)
    //  prevID = (ReformatHolder::ReformatID) pCombo->GetItemData(sel);
    //LOGI("  prevID = %d", prevID);

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
                //LOGI("MATCH at %d (0x%02x)", idIdx, testApplies);
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
                //  LOGI("  Found 'always' prevID, selecting");
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
    LOGD("  At end, sel is %d", sel);
    if (sel != CB_ERR)
        returnID = (ReformatHolder::ReformatID) pCombo->GetItemData(sel);

    return returnID;
}

void ViewFilesDialog::OnFormatSelChange(void)
{
    /*
     * The user has changed entries in the format selection drop box.
     *
     * Also called from the "quick change" buttons.
     */

    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_FVIEW_FORMATSEL);
    ASSERT(pCombo != NULL);
    LOGD("+++ SELECTION IS NOW %d", pCombo->GetCurSel());

    SelectionEntry* pSelEntry = fpSelSet->IterCurrent();
    GenericEntry* pEntry = pSelEntry->GetEntry();
    ReformatHolder::ReformatPart part;
    ReformatHolder::ReformatID id;

    part = GetSelectedPart();
    id = (ReformatHolder::ReformatID) pCombo->GetItemData(pCombo->GetCurSel());
    Reformat(pEntry, part, id);

    DisplayText(pEntry->GetDisplayName(), IsSourceEmpty(pEntry, part));
}

void ViewFilesDialog::OnFviewData(void)
{
    ForkSelectCommon(ReformatHolder::kPartData);
}

void ViewFilesDialog::OnFviewRsrc(void)
{
    ForkSelectCommon(ReformatHolder::kPartRsrc);
}

void ViewFilesDialog::OnFviewCmmt(void)
{
    ForkSelectCommon(ReformatHolder::kPartCmmt);
}

void ViewFilesDialog::ForkSelectCommon(ReformatHolder::ReformatPart part)
{
    GenericEntry* pEntry;
    ReformatHolder::ReformatID id;

    LOGI("Switching to file part=%d", part);
    ASSERT(fpHolder != NULL);
    ASSERT(fpSelSet != NULL);
    ASSERT(fpSelSet->IterCurrent() != NULL);
    pEntry = fpSelSet->IterCurrent()->GetEntry();
    ASSERT(pEntry != NULL);

    id = ConfigureFormatSel(part);

    Reformat(pEntry, part, id);
    DisplayText(pEntry->GetDisplayName(), IsSourceEmpty(pEntry, part));
}

void ViewFilesDialog::OnFviewFmtBest(void)
{
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_FVIEW_FORMATSEL);
    pCombo->SetCurSel(0);       // best is always at the top
    OnFormatSelChange();
    pCombo->SetFocus();
}

void ViewFilesDialog::OnFviewFmtHex(void)
{
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_FVIEW_FORMATSEL);
    int sel = FindByVal(pCombo, ReformatHolder::kReformatHexDump);
    if (sel < 0)
        sel = 0;
    pCombo->SetCurSel(sel);
    OnFormatSelChange();
    pCombo->SetFocus();
}

void ViewFilesDialog::OnFviewFmtRaw(void)
{
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_FVIEW_FORMATSEL);
    int sel = FindByVal(pCombo, ReformatHolder::kReformatRaw);
    if (sel < 0)
        sel = 0;
    pCombo->SetCurSel(sel);
    OnFormatSelChange();
    pCombo->SetFocus();
}

void ViewFilesDialog::EnableFormatSelection(BOOL enable)
{
    GetDlgItem(IDC_FVIEW_FORMATSEL)->EnableWindow(enable);
    GetDlgItem(IDC_FVIEW_FMT_BEST)->EnableWindow(enable);
    GetDlgItem(IDC_FVIEW_FMT_HEX)->EnableWindow(enable);
    GetDlgItem(IDC_FVIEW_FMT_RAW)->EnableWindow(enable);
}

int ViewFilesDialog::FindByVal(CComboBox* pCombo, DWORD val)
{
    int count = pCombo->GetCount();
    int i;

    for (i = 0; i < count; i++) {
        if (pCombo->GetItemData(i) == val)
            return i;
    }
    return -1;
}

void ViewFilesDialog::OnFviewFont(void)
{
    // Choose a font, then apply the choice to all of the text in the box.

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
        LOGI("Now using %d-point '%ls'", fPointSize, (LPCWSTR) fTypeFace);

        NewFontSelected(false);
    }

}

void ViewFilesDialog::NewFontSelected(bool resetBold)
{
    CRichEditCtrl* pEdit = (CRichEditCtrl*) GetDlgItem(IDC_FVIEW_EDITBOX);
    ASSERT(pEdit != NULL);

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


void ViewFilesDialog::OnFviewPrint(void)
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
            LOGI("Default printer generated %d pages", numPages);

            dlg.m_pd.nToPage = dlg.m_pd.nMaxPage = numPages;
        }

        pMainWindow->fhDevMode = dlg.m_pd.hDevMode;
        pMainWindow->fhDevNames = dlg.m_pd.hDevNames;
    }

    long startChar, endChar;
    fEditCtrl.GetSel(/*ref*/startChar, /*ref*/endChar);

    if (endChar != startChar) {
        LOGI("GetSel returned start=%ld end=%ld", startChar, endChar);
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
        CheckedLoadString(&msg, IDS_PRINTER_NOT_USABLE);
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

void ViewFilesDialog::OnFviewFind(void)
{
    // User hit the "Find..." button.  Open the modeless Find dialog.
    DWORD flags = 0;

    if (fpFindDialog != NULL)
        return;

    if (fFindDown)
        flags |= FR_DOWN;
    if (fFindMatchCase)
        flags |= FR_MATCHCASE;
    if (fFindMatchWholeWord)
        flags |= FR_WHOLEWORD;

    fpFindDialog = new CFindReplaceDialog;
    fpFindDialog->Create(TRUE,      // "find" only
                         fFindLastStr,  // default string to search for
                         NULL,      // default string to replace
                         flags,     // flags
                         this);     // parent
}

LRESULT ViewFilesDialog::OnFindDialogMessage(WPARAM wParam, LPARAM lParam)
{
    /*
     * Handle activity in the modeless "find" dialog.
     */

    assert(fpFindDialog != NULL);

    fFindDown = (fpFindDialog->SearchDown() != 0);
    fFindMatchCase = (fpFindDialog->MatchCase() != 0);
    fFindMatchWholeWord = (fpFindDialog->MatchWholeWord() != 0);

    if (fpFindDialog->IsTerminating()) {
        LOGI("VFD find dialog closing");
        fpFindDialog = NULL;
        return 0;
    }

    if (fpFindDialog->FindNext()) {
        fFindLastStr = fpFindDialog->GetFindString();
        FindNext(fFindLastStr, fFindDown, fFindMatchCase, fFindMatchWholeWord);
    } else {
        LOGI("Unexpected find dialog activity");
    }

    return 0;
}


void ViewFilesDialog::FindNext(const WCHAR* str, bool down, bool matchCase,
    bool wholeWord)
{
    LOGI("FindText '%ls' d=%d c=%d w=%d", str, down, matchCase, wholeWord);

    FINDTEXTEX findTextEx = { 0 };
    CHARRANGE selChrg;
    DWORD flags = 0;
    long start, result;

    if (down)
        flags |= FR_DOWN;
    if (matchCase)
        flags |= FR_MATCHCASE;
    if (wholeWord)
        flags |= FR_WHOLEWORD;

    fEditCtrl.GetSel(selChrg);
    if (selChrg.cpMin == selChrg.cpMax)
        start = selChrg.cpMin;      // start at caret
    else
        start = selChrg.cpMin +1;   // start past selection
    LOGD("  selection is %ld,%ld; start=%ld",
        selChrg.cpMin, selChrg.cpMax, start);

    findTextEx.lpstrText = str;
    findTextEx.chrg.cpMin = start;
    if (down) {
        findTextEx.chrg.cpMax = -1;
    } else {
        findTextEx.chrg.cpMax = 0;
    }
    LOGV("  using cpMin=%ld cpMax=%ld",
        findTextEx.chrg.cpMin, findTextEx.chrg.cpMax);

    result = fEditCtrl.FindText(flags, &findTextEx);
    if (result == -1) {
        LOGD("  not found, wrapping and retrying");
        /* didn't find it, wrap around to start/end and retry */
        if (down) {
            findTextEx.chrg.cpMin = 0;
            findTextEx.chrg.cpMax = -1;
        } else {
            findTextEx.chrg.cpMin = fEditCtrl.GetTextLength();
            findTextEx.chrg.cpMax = 0;
        }
        findTextEx.lpstrText = str;
        result = fEditCtrl.FindText(flags, &findTextEx);
    }

    LOGD("  result=%ld min=%ld max=%ld", result,
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
