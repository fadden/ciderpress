/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Goodies needed for printing.
 */
#ifndef APP_PRINT_H
#define APP_PRINT_H

#include "ContentList.h"
#include "resource.h"


/*
 * Printing base class.
 */
class PrintStuff {
protected:
    PrintStuff(void) : fpDC(NULL) {}
    virtual ~PrintStuff(void) {}

    /* get basic goodies, based on the DC */
    virtual void InitBasics(CDC* pDC);

    // Trim a string until it's <= width; returns final width.
    int TrimString(CString* pStr, int width, bool addOnLeft = false);

    // Fills in blank "pFont" object with font that tries to get us N
    //  lines per page.
    void CreateFontByNumLines(CFont* pFont, int numLines);

    int StringWidth(const CString& str);

    static const WCHAR kCourierNew[];
    static const WCHAR kTimesNewRoman[];

    enum {
        kTwipsPerInch = 1440
    };

    /* the printer DC */
    CDC*        fpDC;

    //CFont*        fpOldFont;

    /* some stuff gleaned from fpDC */
    int         fVertRes;
    int         fHorzRes;
    int         fLogPixelsX;
    int         fLogPixelsY;
};


/*
 * Print a content list.
 */
class PrintContentList : public PrintStuff {
public:
    PrintContentList(void) : fpContentList(NULL), fCLLinesPerPage(0) {}
    virtual ~PrintContentList(void) {}

    /* set the DC and the parent window (for the cancel box) */
    virtual void Setup(CDC* pDC, CWnd* pParent);

    int Print(const ContentList* pContentList);
    int Print(const ContentList* pContentList, int fromPage, int toPage);

    /* this is used to set up the page range selection in print dialog */
    static int GetLinesPerPage(void) {
        return kTargetLinesPerPage - kHeaderLines;
    }

private:
    void CalcNumPages(void);
    int StartPrint(void);
    int DoPrint(void);
    void DoPrintPage(int page);

    enum {
        kHeaderLines = 4,           // lines of header stuff on each page
        kTargetLinesPerPage = 64,   // use fLinesPerPage for actual value
    };

    CWnd*       fpParentWnd;
    CFont       fPrintFont;
    int         fLinesPerPage;
    int         fCharHeight;
    int         fEllipsisWidth;

    const ContentList*  fpContentList;
    CString     fDocTitle;
    int         fCLLinesPerPage;        // fLinesPerPage - kHeaderLines
    int         fNumPages;
    int         fFromPage;
    int         fToPage;
};


/*
 * Print the contents of a RichEdit control.
 */
class PrintRichEdit : public PrintStuff {
public:
    PrintRichEdit(void) : fInitialized(false), fpParentWnd(NULL) {}
    virtual ~PrintRichEdit(void) {}

    /* set the DC and the parent window (for the cancel box) */
    virtual void Setup(CDC* pDC, CWnd* pParent);

    /*
     * Commence printing.
     */
    int PrintPreflight(CRichEditCtrl* pREC, int* pNumPages);
    int PrintAll(CRichEditCtrl* pREC, const WCHAR* title);
    int PrintPages(CRichEditCtrl* pREC, const WCHAR* title, int startPage,
        int endPage);
    int PrintSelection(CRichEditCtrl* pREC, const WCHAR* title, long startChar,
        long endChar);

private:
    int StartPrint(CRichEditCtrl* pREC, const WCHAR* title,
        int* pNumPages, bool doPrint);
    void PrintPrep(FORMATRANGE* pFR);
    void ComputeMargins(void);
    int DoPrint(CRichEditCtrl* pREC, const WCHAR* title, int* pNumPages,
        bool doPrint);

    bool        fInitialized;
    CFont       fTitleFont;
    int         fCharHeight;
    int         fLeftMargin;
    int         fRightMargin;

    CWnd*       fpParentWnd;
    int         fStartChar;
    int         fEndChar;
    int         fStartPage;
    int         fEndPage;
};

#endif /*APP_PRINT_H*/
