/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for printing.
 */
#include "stdafx.h"
#include "Print.h"
#include "Main.h"
#include "Preferences.h"


/*
 * ==========================================================================
 *      PrintStuff
 * ==========================================================================
 */

/*static*/ const WCHAR PrintStuff::kCourierNew[] = L"Courier New";
/*static*/ const WCHAR PrintStuff::kTimesNewRoman[] = L"Times New Roman";

/*
 * Set up various values.
 */
void
PrintStuff::InitBasics(CDC* pDC)
{
    ASSERT(pDC != NULL);
    ASSERT(fpDC == NULL);

    fpDC = pDC;

    /* make sure we're in MM_TEXT mode */
    pDC->SetMapMode(MM_TEXT);

    /* get device capabilities; logPixels is e.g. 300 */
    fVertRes = pDC->GetDeviceCaps(VERTRES);
    fHorzRes = pDC->GetDeviceCaps(HORZRES);
    fLogPixelsX = pDC->GetDeviceCaps(LOGPIXELSX);
    fLogPixelsY = pDC->GetDeviceCaps(LOGPIXELSY);
    WMSG4("+++ logPixelsX=%d logPixelsY=%d fHorzRes=%d fVertRes=%d\n",
        fLogPixelsX, fLogPixelsY, fHorzRes, fVertRes);
}

/*
 * Create a new font, based on the number of lines per page we need.
 */
void
PrintStuff::CreateFontByNumLines(CFont* pFont, int numLines)
{
    ASSERT(pFont != NULL);
    ASSERT(numLines > 0);
    ASSERT(fpDC != NULL);

    /* required height */
    int reqCharHeight;
    reqCharHeight = (fVertRes + numLines/2) / numLines;

    /* magic fudge factor */
    int fudge = reqCharHeight / 24;
    WMSG2("  Reducing reqCharHeight from %d to %d\n",
        reqCharHeight, reqCharHeight - fudge);
    reqCharHeight -= fudge;

    pFont->CreateFont(reqCharHeight, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, kTimesNewRoman);
    /*fpOldFont =*/ fpDC->SelectObject(pFont);
}


/*
 * Returns the width of the string.
 */
int
PrintStuff::StringWidth(const CString& str)
{
    CSize size;
    size = fpDC->GetTextExtent(str);
    return size.cx;
}

/*
 * Trim a string to the specified number of pixels.  If it's too large,
 * ellipsis will be added on the left or right.
 */
int
PrintStuff::TrimString(CString* pStr, int width, bool addOnLeft)
{
    static const char* kEllipsis = "...";
    CString newStr;
    int strWidth;
    CSize size;

    size = fpDC->GetTextExtent(kEllipsis);

    if (width < size.cx) {
        ASSERT(false);
        return width;
    }

    newStr = *pStr;

    /*
     * Do a linear search.  This would probably be better served with a
     * binary search or at least a good textmetric-based guess.
     */
    strWidth = StringWidth(newStr);
    while (strWidth > width) {
        if (pStr->IsEmpty()) {
            ASSERT(false);
            return width;
        }
        if (addOnLeft) {
            *pStr = pStr->Right(pStr->GetLength() -1);
            newStr = kEllipsis + *pStr;
        } else {
            *pStr = pStr->Left(pStr->GetLength() -1);
            newStr = *pStr + kEllipsis;
        }

        if (!addOnLeft) {
            WMSG1("Now trying '%ls'\n", (LPCWSTR) newStr);
        }
        strWidth = StringWidth(newStr);
    }

    *pStr = newStr;
    return strWidth;
}


/*
 * ==========================================================================
 *      PrintContentList
 * ==========================================================================
 */

/*
 * Calculate some constant values.
 */
void
PrintContentList::Setup(CDC* pDC, CWnd* pParent)
{
    /* init base class */
    InitBasics(pDC);

    /* init our stuff */
    CreateFontByNumLines(&fPrintFont, kTargetLinesPerPage);

    fpParentWnd = pParent;

    /* compute text metrics */
    TEXTMETRIC metrics;
    pDC->GetTextMetrics(&metrics);
    fCharHeight = metrics.tmHeight + metrics.tmExternalLeading;
    fLinesPerPage = fVertRes / fCharHeight;

    WMSG2("fVertRes=%d, fCharHeight=%d\n", fVertRes, fCharHeight);

    /* set up our slightly reduced lines per page */
    ASSERT(fLinesPerPage > kHeaderLines+1);
    fCLLinesPerPage = fLinesPerPage - kHeaderLines;
}

/*
 * Compute the number of pages in fpContentList.
 */
void
PrintContentList::CalcNumPages(void)
{
    /* set up our local goodies */
    ASSERT(fpContentList != NULL);
    int numLines = fpContentList->GetItemCount();
    ASSERT(numLines > 0);

    fNumPages = (numLines + fCLLinesPerPage -1) / fCLLinesPerPage;
    ASSERT(fNumPages > 0);

    WMSG3("Using numLines=%d, fNumPages=%d, fCLLinesPerPage=%d\n",
        numLines, fNumPages, fCLLinesPerPage);
}

/*
 * Initiate printing of the specified list to the specified DC.
 *
 * Returns 0 if all went well, nonzero on cancellation or failure.
 */
int
PrintContentList::Print(const ContentList* pContentList)
{
    fpContentList = pContentList;
    CalcNumPages();

    fFromPage = 1;
    fToPage = fNumPages;
    return StartPrint();
}
int
PrintContentList::Print(const ContentList* pContentList, int fromPage, int toPage)
{
    fpContentList = pContentList;
    CalcNumPages();

    fFromPage = fromPage;
    fToPage = toPage;
    return StartPrint();
}

/*
 * Kick off the print job.
 */
int
PrintContentList::StartPrint(void)
{
    MainWindow* pMain = (MainWindow*)::AfxGetMainWnd();
    BOOL bres;
    int jobID;
    int result = -1;

    ASSERT(fFromPage >= 1);
    ASSERT(fToPage <= fNumPages);

    // clear the abort flag
    pMain->SetAbortPrinting(false);

    // obstruct input to the main window
    fpParentWnd->EnableWindow(FALSE);

    // create a print-cancel dialog
//  PrintCancelDialog* pPCD = new PrintCancelDialog;
    CancelDialog* pPCD = new CancelDialog;
    bres = pPCD->Create(&pMain->fAbortPrinting, IDD_PRINT_CANCEL, fpParentWnd);
    if (bres == FALSE) {
        WMSG0("WARNING: PrintCancelDialog init failed\n");
    } else {
        fpDC->SetAbortProc(pMain->PrintAbortProc);
    }

    fDocTitle = pMain->GetPrintTitle();

    // set up the print job
    CString printTitle;
    printTitle.LoadString(IDS_PRINT_CL_JOB_TITLE);
    DOCINFO di;
    ::ZeroMemory(&di, sizeof(DOCINFO));
    di.cbSize = sizeof(DOCINFO);
    di.lpszDocName = printTitle;

    jobID = fpDC->StartDoc(&di);
    if (jobID <= 0) {
        WMSG0("Got invalid jobID from StartDoc\n");
        goto bail;
    }
    WMSG1("Got jobID=%d\n", jobID);

    // do the printing
    if (DoPrint() != 0) {
        WMSG0("Printing was aborted\n");
        fpDC->AbortDoc();
    } else {
        WMSG0("Printing was successful\n");
        fpDC->EndDoc();
        result = 0;
    }

bail:
    // destroy print-cancel dialog and restore main window
    fpParentWnd->EnableWindow(TRUE);
    //fpParentWnd->SetActiveWindow();
    if (pPCD != NULL)
        pPCD->DestroyWindow();

    return result;
}


/*
 * Print all pages.
 *
 * Returns 0 on success, nonzero on failure.
 */
int
PrintContentList::DoPrint(void)
{
    WMSG2("Printing from page=%d to page=%d\n", fFromPage, fToPage);

    for (int page = fFromPage; page <= fToPage; page++) {
        if (fpDC->StartPage() <= 0) {
            WMSG0("StartPage returned <= 0, returning -1\n");
            return -1;
        }

        DoPrintPage(page);

        // delay so we can test "cancel" button
//      {
//          MainWindow* pMain = (MainWindow*)::AfxGetMainWnd();
//          pMain->EventPause(1000);
//      }


        if (fpDC->EndPage() <= 0) {
            WMSG0("EndPage returned <= 0, returning -1\n");
            return -1;
        }
    }

    return 0;
}

/*
 * Print page N of the content list, where N is a 1-based count.
 */
void
PrintContentList::DoPrintPage(int page)
{
    /*
     * Column widths, on an arbitrary scale.  These will be
     * scaled appropriately for the page resolution.
     */
    static const struct {
        const char*     name;
        int             width;
        bool            rightJust;
    } kColumnWidths[kNumVisibleColumns] = {
        { "Pathname",   250, false },   // 200
        { "Type",       40, false },    // 44
        { "Auxtype",    47, false },    // 42
        { "Mod Date",   96, false },    // 99
        { "Format",     52, false },    // 54
        { "Size",       55, true },     // 60
        { "Ratio",      40, true },     // 41
        { "Packed",     55, true },     // 60
        { "Access",     39, false },    // 53 upper, 45 lower
    };
    const int kBorderWidth = 3;

    /* normalize */
    float widthMult;
    int totalWidth;

    totalWidth = 0;
    for (int i = 0; i < NELEM(kColumnWidths); i++)
        totalWidth += kColumnWidths[i].width;

    widthMult = (float) fHorzRes / totalWidth;
    WMSG3("totalWidth=%d, fHorzRes=%d, mult=%.3f\n",
        totalWidth, fHorzRes, widthMult);

    /*
     * Calculate some goodies.
     */
    int start, end;
    start = (page-1) * fCLLinesPerPage;
    end = start + fCLLinesPerPage;
    if (end >= fpContentList->GetItemCount())
        end = fpContentList->GetItemCount()-1;

    int offset, nextOffset, cellWidth, border;
    border = (int) (kBorderWidth * widthMult);

    /*
     * Print page header.
     */
    fpDC->TextOut(0, 1 * fCharHeight, fDocTitle);
    CString pageNum;
    pageNum.Format(L"Page %d/%d", page, fNumPages);
    int pageNumWidth = StringWidth(pageNum);
    fpDC->TextOut(fHorzRes - pageNumWidth, 1 * fCharHeight, pageNum);

    /*
     * Print data.
     */
    for (int row = -1; row < fCLLinesPerPage && start + row <= end; row++) {
        CString text;
        offset = 0;

        for (int col = 0; col < kNumVisibleColumns; col++) {
            cellWidth = (int) ((float)kColumnWidths[col].width * widthMult);
            nextOffset = offset + cellWidth;
            if (col != kNumVisibleColumns-1)
                cellWidth -= border;
            if (col == 0)
                cellWidth -= (border*2);    // extra border on pathname

            int yOffset;
            if (row == -1) {
                text = kColumnWidths[col].name;
                yOffset = (row+kHeaderLines-1) * fCharHeight;
            } else {
                text = fpContentList->GetItemText(start + row, col);
                yOffset = (row+kHeaderLines) * fCharHeight;
            }

            int strWidth;
            strWidth = TrimString(&text, cellWidth, col == 0);

            if (kColumnWidths[col].rightJust)
                fpDC->TextOut((offset + cellWidth) - strWidth, yOffset, text);
            else
                fpDC->TextOut(offset, yOffset, text);

            offset = nextOffset;
        }
    }

    /*
     * Add some fancy lines.
     */
    CPen penBlack(PS_SOLID, 1, RGB(0, 0, 0));
    CPen* pOldPen = fpDC->SelectObject(&penBlack);
    fpDC->MoveTo(0, (int) (fCharHeight * (kHeaderLines - 0.5)));
    fpDC->LineTo(fHorzRes, (int) (fCharHeight * (kHeaderLines - 0.5)));

    //fpDC->MoveTo(0, 0);
    //fpDC->LineTo(fHorzRes, fVertRes);
    //fpDC->MoveTo(fHorzRes-1, 0);
    //fpDC->LineTo(0, fVertRes);

    fpDC->SelectObject(pOldPen);
}


/*
 * ==========================================================================
 *      PrintRichEdit
 * ==========================================================================
 */

/*
 * Calculate some constant values.
 */
void
PrintRichEdit::Setup(CDC* pDC, CWnd* pParent)
{
    /* preflighting can cause this to be initialized twice */
    fpDC = NULL;

    /* init base class */
    InitBasics(pDC);

    if (!fInitialized) {
        /*
         * Find a nice font for the title area.
         */
        const int kPointSize = 10;
        int fontHeight;
        BOOL result;

        fontHeight = -MulDiv(kPointSize, fLogPixelsY, 72);

        result = fTitleFont.CreateFont(fontHeight, 0, 0, 0, FW_NORMAL, 0, 0, 0,
            DEFAULT_CHARSET, OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, kTimesNewRoman);
        ASSERT(result);     // everybody has Times New Roman
    }

    fpParentWnd = pParent;
    fInitialized = true;
}

/*
 * Pre-flight the print process to get the number of pages.
 */
int
PrintRichEdit::PrintPreflight(CRichEditCtrl* pREC, int* pNumPages)
{
    fStartChar = 0;
    fEndChar = -1;
    fStartPage = 0;
    fEndPage = -1;
    return StartPrint(pREC, L"(test)", pNumPages, false);
}

/*
 * Print all pages.
 */
int
PrintRichEdit::PrintAll(CRichEditCtrl* pREC, const WCHAR* title)
{
    fStartChar = 0;
    fEndChar = -1;
    fStartPage = 0;
    fEndPage = -1;
    return StartPrint(pREC, title, NULL, true);
}

/*
 * Print a range of pages.
 */
int
PrintRichEdit::PrintPages(CRichEditCtrl* pREC, const WCHAR* title,
    int startPage, int endPage)
{
    fStartChar = 0;
    fEndChar = -1;
    fStartPage = startPage;
    fEndPage = endPage;
    return StartPrint(pREC, title, NULL, true);
}

/*
 * Print the selected area.
 */
int
PrintRichEdit::PrintSelection(CRichEditCtrl* pREC, const WCHAR* title,
    long startChar, long endChar)
{
    fStartChar = startChar;
    fEndChar = endChar;
    fStartPage = 0;
    fEndPage = -1;
    return StartPrint(pREC, title, NULL, true);
}

/*
 * Start the printing process by posting a print-cancel dialog.
 */
int
PrintRichEdit::StartPrint(CRichEditCtrl* pREC, const WCHAR* title,
    int* pNumPages, bool doPrint)
{
    CancelDialog* pPCD = NULL;
    MainWindow* pMain = (MainWindow*)::AfxGetMainWnd();
    int result;

    /* set up the print cancel dialog */
    if (doPrint) {
        BOOL bres;

        /* disable main UI */
        fpParentWnd->EnableWindow(FALSE);

        pPCD = new CancelDialog;
        bres = pPCD->Create(&pMain->fAbortPrinting, IDD_PRINT_CANCEL,
                    fpParentWnd);

        /* set up the DC's print abort callback */
        if (bres != FALSE)
            fpDC->SetAbortProc(pMain->PrintAbortProc);
    }

    result = DoPrint(pREC, title, pNumPages, doPrint);

    if (doPrint) {
        fpParentWnd->EnableWindow(TRUE);
        if (pPCD != NULL)
            pPCD->DestroyWindow();
    }

    return result;
}

/*
 * Do some prep work before printing.
 */
void
PrintRichEdit::PrintPrep(FORMATRANGE* pFR)
{
    CFont* pOldFont;

    /* make sure we're in MM_TEXT mode */
    fpDC->SetMapMode(MM_TEXT);

    TEXTMETRIC metrics;
    pOldFont = fpDC->SelectObject(&fTitleFont);
    fpDC->GetTextMetrics(&metrics);
    fCharHeight = metrics.tmHeight + metrics.tmExternalLeading;
    fpDC->SelectObject(pOldFont);
    //WMSG1("CHAR HEIGHT is %d\n", fCharHeight);

    /* compute fLeftMargin and fRightMargin */
    ComputeMargins();

    /*
     * Set up the FORMATRANGE values.  The Rich Edit stuff likes to have
     * measurements in TWIPS, whereas the printer is using DC pixel
     * values.  fLogPixels_ tells us how many pixels per inch.
     */
    memset(pFR, 0, sizeof(FORMATRANGE));
    pFR->hdc = pFR->hdcTarget = fpDC->m_hDC;

    /*
     * Set frame for printable area, in TWIPS.  The printer DC will set
     * its own "reasonable" margins, so the area here is not the entire
     * sheet of paper.
     */
    pFR->rcPage.left = pFR->rcPage.top = 0;
    pFR->rcPage.right = (fHorzRes * kTwipsPerInch) / fLogPixelsX;
    pFR->rcPage.bottom = (fVertRes * kTwipsPerInch) / fLogPixelsY;

    long topOffset = (long) ((fCharHeight * 1.5 * kTwipsPerInch) / fLogPixelsY);
    pFR->rc.top = pFR->rcPage.top + topOffset;
    pFR->rc.bottom = pFR->rcPage.bottom;
    pFR->rc.left = pFR->rcPage.left + (fLeftMargin * kTwipsPerInch) / fLogPixelsX;
    pFR->rc.right = pFR->rcPage.right - (fRightMargin * kTwipsPerInch) / fLogPixelsX;

    WMSG2("PRINTABLE AREA is %d wide x %d high (twips)\n",
        pFR->rc.right - pFR->rc.left, pFR->rc.bottom - pFR->rc.top);
    WMSG2("FRAME is %d wide x %d high (twips)\n",
        pFR->rcPage.right - pFR->rcPage.left, pFR->rcPage.bottom - pFR->rcPage.top);

    pFR->chrg.cpMin = fStartChar;
    pFR->chrg.cpMax = fEndChar;
}

/*
 * Compute the size of the left and right margins, based on the width of 80
 * characters of 10-point Courier New on the current printer.
 *
 * Sets fLeftMargin and fRightMargin, in printer DC pixels.
 */
void
PrintRichEdit::ComputeMargins(void)
{
    CFont tmpFont;
    CFont* pOldFont;
    int char80width, fontHeight, totalMargin;
    BOOL result;

    fontHeight = -MulDiv(10, fLogPixelsY, 72);

    result = tmpFont.CreateFont(fontHeight, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, kCourierNew);
    ASSERT(result);

    pOldFont = fpDC->SelectObject(&tmpFont);
    // in theory we could compute one 'X' * 80; this seems more reliable
    WCHAR str[81];
    for (int i = 0; i < 80; i++)
        str[i] = 'X';
    str[80] = '\0';
    char80width = StringWidth(str);
    fpDC->SelectObject(pOldFont);

    //WMSG1("char80 string width=%d\n", char80width);

    /*
     * If there's not enough room on the page, set the margins to zero.
     * If the margins required exceed two inches, just set the margin
     * to one inch on either side.
     */
    totalMargin = fHorzRes - char80width;
    if (totalMargin < 0) {
        WMSG0(" Page not wide enough, setting margins to zero\n");
        fLeftMargin = fRightMargin = 0;
    } else if (totalMargin > fLogPixelsX * 2) {
        WMSG0("  Page too wide, setting margins to 1 inch\n");
        fLeftMargin = fRightMargin = fLogPixelsX;
    } else {
        // try to get leftMargin equal to 1/2"
        fLeftMargin = totalMargin / 2;
        if (fLeftMargin > fLogPixelsX / 2)
            fLeftMargin = fLogPixelsX / 2;
        fRightMargin = totalMargin - fLeftMargin -1;
        WMSG3("  +++ Margins (in %d pixels/inch) are left=%ld right=%ld\n",
            fLogPixelsX, fLeftMargin, fRightMargin);
    }
}

/*
 * Send the contents of the rich edit control to the printer DC.
 *
 * This was derived from Microsft KB article 129860.
 */
int
PrintRichEdit::DoPrint(CRichEditCtrl* pREC, const WCHAR* title,
    int* pNumPages, bool doPrint)
{
    FORMATRANGE fr;
    DOCINFO di;
    long textLength, textPrinted, lastTextPrinted;
    int pageNum;

    WMSG2("DoPrint: title='%ls' doPrint=%d\n", title, doPrint);
    WMSG4("         startChar=%d endChar=%d startPage=%d endPage=%d\n",
        fStartChar, fEndChar, fStartPage, fEndPage);

    /*
     * Get the title font and fill out the FORMATRANGE structure.
     */
    PrintPrep(&fr);

    /* fill out a DOCINFO */
    memset(&di, 0, sizeof(di));
    di.cbSize = sizeof(DOCINFO);
    di.lpszDocName = title;

    if (doPrint)
        fpDC->StartDoc(&di);

    /*
     * Here's the really strange part of the affair.  The GetTextLength call
     * shown in the MSFT KB article doesn't return the correct number of
     * characters.  The CRichEditView code in MFC uses a GetTextLengthEx
     * call, which is documented as being part of the CRichEditCtrl but
     * doesn't appear in the header files.  Call it the hard way.
     *
     * If you print a "raw" file with carriage returns, and you use version
     * 5.30.23.1200 of "riched20.dll", you get "9609" from GetTextLength and
     * "9528" from GetTextLengthEx when the document's length is 9528 and
     * there are 81 carriage returns.  The value you want to use is 9528,
     * because the print code doesn't double-up the count on CRs.
     *
     * If instead you use version 5.30.23.1215, you get the same answer
     * from both calls, and printing works fine.
     *
     * GetTextLengthEx is part of "riched20.dll".  Win9x uses "riched32.dll",
     * which doesn't support the call.
     */
    GETTEXTLENGTHEX exLenReq;
    long basicTextLength, extdTextLength;
    basicTextLength = pREC->GetTextLength();
    exLenReq.flags = GTL_PRECISE | GTL_NUMCHARS;
#ifdef _UNICODE
    exLenReq.codepage = 1200;   // UTF-16 little-endian
#else
    exLenReq.codepage = CP_ACP;
#endif
    extdTextLength = (long)::SendMessage(pREC->m_hWnd, EM_GETTEXTLENGTHEX,
        (WPARAM) &exLenReq, (LPARAM) NULL);
    WMSG2("RichEdit text length: std=%ld extd=%ld\n",
        basicTextLength, extdTextLength);

    if (fEndChar == -1) {
        if (extdTextLength > 0)
            textLength = extdTextLength;
        else
            textLength = basicTextLength;
    } else
        textLength = fEndChar - fStartChar;

    WMSG1("  +++ starting while loop, textLength=%ld\n", textLength);
    pageNum = 0;
    lastTextPrinted = -1;
    do {
        bool skipPage = false;
        pageNum++;
        WMSG1("  +++  while loop: pageNum is %d\n", pageNum);

        if (fEndPage > 0) {
            if (pageNum < fStartPage)
                skipPage = true;
            if (pageNum > fEndPage)
                break;  // out of while, ending print
        }

        if (doPrint && !skipPage) {
            fpDC->StartPage();

            CFont* pOldFont = fpDC->SelectObject(&fTitleFont);
            fpDC->TextOut(0, 0 * fCharHeight, title);
            CString pageNumStr;
            pageNumStr.Format(L"Page %d", pageNum);
            int pageNumWidth = StringWidth(pageNumStr);
            fpDC->TextOut(fHorzRes - pageNumWidth, 0 * fCharHeight, pageNumStr);
            fpDC->SelectObject(pOldFont);

            CPen penBlack(PS_SOLID, 1, RGB(0, 0, 0));
            CPen* pOldPen = fpDC->SelectObject(&penBlack);
            int ycoord = (int) (fCharHeight * 1.25);
            fpDC->MoveTo(0, ycoord-1);
            fpDC->LineTo(fHorzRes, ycoord-1);
            fpDC->MoveTo(0, ycoord);
            fpDC->LineTo(fHorzRes, ycoord);

            fpDC->SelectObject(pOldPen);
        }

        //WMSG1("  +++   calling FormatRange(%d)\n", doPrint && !skipPage);
        //LogHexDump(&fr, sizeof(fr));

        /* print a page full of RichEdit stuff */
        textPrinted = pREC->FormatRange(&fr, doPrint && !skipPage);
        WMSG1("  +++    returned from FormatRange (textPrinted=%d)\n",
            textPrinted);
        if (textPrinted <= lastTextPrinted) {
            /* the earlier StartPage can't be undone, so we'll get an
               extra blank page at the very end */
            WMSG3("GLITCH: no new text printed (printed=%ld, last=%ld, len=%ld)\n",
                textPrinted, lastTextPrinted, textLength);
            pageNum--;  // fix page count estimator
            break;
        }
        lastTextPrinted = textPrinted;

        // delay so we can test "cancel" button
        //((MainWindow*)::AfxGetMainWnd())->EventPause(1000);

        if (doPrint && !skipPage) {
            if (fpDC->EndPage() <= 0) {
                /* the "cancel" button was hit */
                WMSG0("EndPage returned <= 0 (cancelled)\n");
                fpDC->AbortDoc();
                return -1;
            }
        }

        if (textPrinted < textLength) {
            fr.chrg.cpMin = textPrinted;
            fr.chrg.cpMax = fEndChar;   // -1 if nothing selected
        }
    } while (textPrinted < textLength);

    //WMSG0("  +++ calling FormatRange(NULL, FALSE)\n");
    pREC->FormatRange(NULL, FALSE);
    //WMSG0("  +++  returned from final FormatRange\n");

    if (doPrint)
        fpDC->EndDoc();

    if (pNumPages != NULL)
        *pNumPages = pageNum;

    WMSG1("Printing completed (textPrinted=%ld)\n", textPrinted);

    return 0;
}
