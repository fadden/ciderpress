/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Miscellaneous utility classes.
 */
#include "stdafx.h"
#include "Util.h"
#include <stdarg.h>


/*
 * ===========================================================================
 *      CGripper
 * ===========================================================================
 */

/*
 * Fake out the hit testing so that it thinks we're over the scrollable bit
 * when we're over the gripper.
 */
BEGIN_MESSAGE_MAP(CGripper, CScrollBar)
    ON_WM_NCHITTEST()
END_MESSAGE_MAP()

LRESULT CGripper::OnNcHitTest(CPoint point) 
{
    UINT ht = CScrollBar::OnNcHitTest(point);
    if (ht == HTCLIENT) {
        ht = HTBOTTOMRIGHT;
    }
    return ht;
}


/*
 * ===========================================================================
 *      RichEditXfer
 * ===========================================================================
 */

/*
 * At the behest of a RichEditCtrl, copy "cb" bytes of data into "pbBuff".
 * The actual number of bytes transferred is placed in "*pcb".
 *
 * (A complete implementation would allow copying either direction.)
 *
 * Returns 0 on success, nonzero on error.
 */
DWORD RichEditXfer::EditStreamCallback(DWORD dwCookie, LPBYTE pbBuff,
    LONG cb, LONG* pcb)
{
    RichEditXfer* pThis = (RichEditXfer*) dwCookie;

    ASSERT(dwCookie != NULL);
    ASSERT(pbBuff != NULL);
    ASSERT(cb != 0);
    ASSERT(pcb != NULL);

    long copyLen = pThis->fLen;
    if (copyLen > cb)
        copyLen = cb;
    if (copyLen == 0) {
        ASSERT(pThis->fLen == 0);
        goto bail;
    }

    ::memcpy(pbBuff, pThis->fBuf, copyLen);

    pThis->fBuf += copyLen;
    pThis->fLen -= copyLen;

    //LOGI("ESC: copyLen=%d, now fLen=%d", copyLen, pThis->fLen);

bail:
    *pcb = copyLen;
    return 0;
}


/*
 * ===========================================================================
 *      ExpandBuffer
 * ===========================================================================
 */

int ExpandBuffer::CreateWorkBuf(void)
{
    if (fWorkBuf != NULL) {
        ASSERT(fWorkMax > 0);
        return 0;
    }

    assert(fInitialSize > 0);

    fWorkBuf = new char[fInitialSize];
    if (fWorkBuf == NULL)
        return -1;

    fWorkCount = 0;
    fWorkMax = fInitialSize;

    return 0;
}

void ExpandBuffer::SeizeBuffer(char** ppBuf, long* pLen)
{
    *ppBuf = fWorkBuf;
    *pLen = fWorkCount;

    fWorkBuf = NULL;    // discard pointer so we don't free it
    fWorkCount = 0;
    fWorkMax = 0;
}

int ExpandBuffer::GrowWorkBuf(void)
{
    int newIncr = fWorkMax;
    if (newIncr > kWorkBufMaxIncrement)
        newIncr = kWorkBufMaxIncrement;

    LOGV("Extending buffer by %d (count=%d, max=%d)",
        newIncr, fWorkCount, fWorkMax);

    fWorkMax += newIncr;

    /* debug-only check to catch runaways */
//  ASSERT(fWorkMax < 1024*1024*24);

    char* newBuf = new char[fWorkMax];
    if (newBuf == NULL) {
        LOGE("ALLOC FAILURE (%ld)", fWorkMax);
        ASSERT(false);
        fWorkMax -= newIncr;    // put it back so we don't overrun
        return -1;
    }

    memcpy(newBuf, fWorkBuf, fWorkCount);
    delete[] fWorkBuf;
    fWorkBuf = newBuf;

    return 0;
}

void ExpandBuffer::Write(const unsigned char* buf, long len)
{
    if (fWorkBuf == NULL)
        CreateWorkBuf();
    while (fWorkCount + len >= fWorkMax) {
        if (GrowWorkBuf() != 0)
            return;
    }
    ASSERT(fWorkCount + len < fWorkMax);
    memcpy(fWorkBuf + fWorkCount, buf, len);
    fWorkCount += len;
}

void ExpandBuffer::Putc(char ch)
{
    Write((const unsigned char*) &ch, 1);
}

void ExpandBuffer::Printf(_Printf_format_string_ const char* format, ...)
{
    va_list args;

    ASSERT(format != NULL);

    if (fWorkBuf == NULL)
        CreateWorkBuf();

    va_start(args, format);

    if (format != NULL) {
        int count;
        count = _vsnprintf(fWorkBuf + fWorkCount, fWorkMax - fWorkCount,
                    format, args);
        if (count < 0) {
            if (GrowWorkBuf() != 0)
                return;

            /* try one more time, then give up */
            count = _vsnprintf(fWorkBuf + fWorkCount, fWorkMax - fWorkCount,
                    format, args);
            ASSERT(count >= 0);
            if (count < 0)
                return;
        }

        fWorkCount += count;
        ASSERT(fWorkCount <= fWorkMax);
    }

    va_end(args);
}


/*
 * ===========================================================================
 *      Windows helpers
 * ===========================================================================
 */

void EnableControl(CDialog* pDlg, int id, bool enable)
{
    CWnd* pWnd = pDlg->GetDlgItem(id);
    if (pWnd == NULL) {
        LOGI("GLITCH: control %d not found in dialog", id);
        ASSERT(false);
    } else {
        pWnd->EnableWindow(enable);
    }
}

void MoveControl(CDialog* pDlg, int id, int deltaX, int deltaY, bool redraw)
{
    CWnd* pWnd;
    CRect rect;

    pWnd = pDlg->GetDlgItem(id);
    ASSERT(pWnd != NULL);
    if (pWnd == NULL)
        return;

    pWnd->GetWindowRect(&rect);
    pDlg->ScreenToClient(&rect);
    rect.left += deltaX;
    rect.right += deltaX;
    rect.top += deltaY;
    rect.bottom += deltaY;
    pWnd->MoveWindow(&rect, redraw);
}

void StretchControl(CDialog* pDlg, int id, int deltaX, int deltaY, bool redraw)
{
    CWnd* pWnd;
    CRect rect;

    pWnd = pDlg->GetDlgItem(id);
    ASSERT(pWnd != NULL);
    if (pWnd == NULL)
        return;

    pWnd->GetWindowRect(&rect);
    pDlg->ScreenToClient(&rect);
    rect.right += deltaX;
    rect.bottom += deltaY;
    pWnd->MoveWindow(&rect, redraw);
}

void
MoveStretchControl(CDialog* pDlg, int id, int moveX, int moveY,
    int stretchX, int stretchY, bool redraw)
{
    MoveControl(pDlg, id, moveX, moveY, redraw);
    StretchControl(pDlg, id, stretchX, stretchY, redraw);
}

HDWP MoveControl(HDWP hdwp, CDialog* pDlg, int id, int deltaX, int deltaY,
    bool redraw)
{
    CWnd* pWnd;
    CRect rect;

    pWnd = pDlg->GetDlgItem(id);
    ASSERT(pWnd != NULL);
    if (pWnd == NULL)
        return hdwp;

    pWnd->GetWindowRect(&rect);
    pDlg->ScreenToClient(&rect);
    rect.left += deltaX;
    rect.right += deltaX;
    rect.top += deltaY;
    rect.bottom += deltaY;
    hdwp = DeferWindowPos(hdwp, pWnd->m_hWnd, NULL, rect.left, rect.top,
        rect.Width(), rect.Height(), 0);

    return hdwp;
}

HDWP StretchControl(HDWP hdwp, CDialog* pDlg, int id, int deltaX, int deltaY,
    bool redraw)
{
    CWnd* pWnd;
    CRect rect;

    pWnd = pDlg->GetDlgItem(id);
    ASSERT(pWnd != NULL);
    if (pWnd == NULL)
        return hdwp;

    pWnd->GetWindowRect(&rect);
    pDlg->ScreenToClient(&rect);
    rect.right += deltaX;
    rect.bottom += deltaY;
    hdwp = DeferWindowPos(hdwp, pWnd->m_hWnd, NULL, rect.left, rect.top,
        rect.Width(), rect.Height(), 0);

    return hdwp;
}

HDWP MoveStretchControl(HDWP hdwp, CDialog* pDlg, int id, int moveX, int moveY,
    int stretchX, int stretchY, bool redraw)
{
    CWnd* pWnd;
    CRect rect;

    pWnd = pDlg->GetDlgItem(id);
    ASSERT(pWnd != NULL);
    if (pWnd == NULL)
        return hdwp;

    pWnd->GetWindowRect(&rect);
    pDlg->ScreenToClient(&rect);
    rect.left += moveX;
    rect.right += moveX;
    rect.top += moveY;
    rect.bottom += moveY;
    rect.right += stretchX;
    rect.bottom += stretchY;
    hdwp = DeferWindowPos(hdwp, pWnd->m_hWnd, NULL, rect.left, rect.top,
        rect.Width(), rect.Height(), 0);

    return hdwp;
}

int GetDlgButtonCheck(CWnd* pWnd, int id)
{
    CButton* pButton;
    pButton = (CButton*) pWnd->GetDlgItem(id);
    ASSERT(pButton != NULL);
    if (pButton == NULL)
        return -1;
    return pButton->GetCheck();
}

void SetDlgButtonCheck(CWnd* pWnd, int id, int checkVal)
{
    CButton* pButton;
    pButton = (CButton*) pWnd->GetDlgItem(id);
    ASSERT(pButton != NULL);
    if (pButton == NULL)
        return;
    pButton->SetCheck(checkVal);
}

void CreateSimpleFont(CFont* pFont, CWnd* pWnd, const WCHAR* typeFace,
        int pointSize)
{
    CClientDC dc(pWnd);
    int height;

    height = -((dc.GetDeviceCaps(LOGPIXELSY) * pointSize) / 72);
    pFont->CreateFont(height, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, typeFace);
}

void GetWin32ErrorString(DWORD err, CString* pStr)
{
    DWORD count;
    LPVOID lpMsgBuf;

    count = FormatMessage( 
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM | 
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        err /*GetLastError()*/,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
        (LPTSTR) &lpMsgBuf,
        0,
        NULL 
    );

    if (!count) {
        LOGI("FormatMessage on err=0x%08lx failed", err);
        pStr->Format(L"system error 0x%08lx.\n", err);
    } else {
        *pStr = (LPCTSTR)lpMsgBuf;
        //MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
        LocalFree( lpMsgBuf );
    }
}

void ShowFailureMsg(CWnd* pWnd, const CString& msg, int titleStrID)
{
    CString title;

    CheckedLoadString(&title, titleStrID);
    pWnd->MessageBox(msg, title, MB_OK | MB_ICONERROR);
}

void ShowFailureMsg(CWnd* pWnd, int msgId, int titleStrID)
{
    CString msg, title;

    CheckedLoadString(&title, titleStrID);
    CheckedLoadString(&msg, msgId);
    pWnd->MessageBox(msg, title, MB_OK | MB_ICONERROR);
}


bool IsWin9x(void)
{
    return false;
}

void CheckedLoadString(CString* pString, UINT nID)
{
    if (!pString->LoadString(nID)) {
        LOGW("WARNING: failed to load string %u", nID);
        *pString = L"!Internal failure!";
    }
}


/*
 * ===========================================================================
 *      Miscellaneous
 * ===========================================================================
 */

int GetPascalString(const uint8_t* buf, long maxLen, CString* pStr)
{
    int len = *buf++;
    int retLen = len;
    *pStr = "";

    if (len > maxLen) {
        LOGW("Invalid pascal string -- len=%d, maxLen=%d", len, maxLen);
        return -1;
    }

    while (len--) {
        if (*buf == '\0') {
            /* this suggests that we're not reading a pascal string */
            LOGW("Found pascal string with '\\0' in it");
            return -1;
        }

        /* not the fastest approach, but it'll do */
        *pStr += *buf++;
    }

    return retLen;
}

void LogHexDump(const void* vbuf, long len)
{
    const unsigned char* buf = (const unsigned char*) vbuf;
    char outBuf[10 + 16*3 +1 +8];   // addr: 00 11 22 ... + 8 bytes slop
    bool skipFirst;
    uintptr_t addr;
    char* cp = NULL;
    int i;

    LOGI(" Memory at 0x%p %ld bytes:", buf, len);
    if (len <= 0)
        return;

    addr = (uintptr_t)buf & ~0x0f;
    if (addr != (uintptr_t) buf) {
        sprintf(outBuf, "%08lx: ", addr);
        for (i = addr; i < (int) buf; i++)
            strcat(outBuf, "   ");
        cp = outBuf + strlen(outBuf);
        skipFirst = false;
    } else {
        skipFirst = true;
    }

    while (len--) {
        if (((uintptr_t) buf & 0x0f) == 0) {
            /* output the old, start a new line */
            if (skipFirst) {
                skipFirst = false;
            } else {
                LOGI("  %hs", outBuf);
                addr += 16;
            }
            sprintf(outBuf, "%08lx: ", addr);
            cp = outBuf + strlen(outBuf);
        }

        sprintf(cp, "%02x ", *buf++);
        cp += 3;
    }

    /* output whatever is left */
    LOGI("  %hs", outBuf);
}

int ComputePercent(LONGLONG part, LONGLONG full)
{
    LONGLONG perc;

    if (!part && !full)
        return 100;     /* file is zero bytes long */

    if (part < 21474836)
        perc = (part * 100) / full;
    else
        perc = part / (full/100);

    /* don't say "0%" if it's not actually zero... it looks dumb */
    if (!perc && full)
        perc = 1;

    return (int) perc;
}

void FormatDate(time_t when, CString* pStr)
{
    if (when == kDateNone) {
        *pStr = L"[No Date]";
    } else if (when == kDateInvalid) {
        *pStr = L"<invalid>";
    } else {
        CTime modWhen(when);
        *pStr = modWhen.Format(L"%d-%b-%y %H:%M");
    }
}

const WCHAR* Stristr(const WCHAR* string1, const WCHAR* string2)
{
    WCHAR *cp1 = (WCHAR*)string1, *cp2, *cp1a;
    WCHAR first;              // get the first char in string to find

    first = string2[0];      // first char often won't be alpha
    if (iswalpha(first))     {
        first = towlower(first);
        for ( ; *cp1  != '\0'; cp1++)
        {
            if (towlower(*cp1) == first)
            {
                for (cp1a = &cp1[1], cp2 = (WCHAR*) &string2[1];;
                cp1a++, cp2++)
                { 
                    if (*cp2 == '\0')
                        return cp1;
                    if (towlower(*cp1a) != towlower(*cp2))
                        break;
                }
            }
        }
    }
    else
    {
        for ( ; *cp1 != '\0' ; cp1++)
        {
            if (*cp1 == first)
            {
                for (cp1a = &cp1[1], cp2 = (WCHAR*) &string2[1];;
                cp1a++, cp2++)
                {
                    if (*cp2 == '\0')
                        return cp1;
                    if (towlower(*cp1a) != towlower(*cp2))
                        break;
                }
            }
        }
    }
    return NULL;
}

void VectorizeString(WCHAR* mangle, WCHAR** argv, int* pArgc)
{
    bool inWhiteSpace = true;
    bool inQuote = false;
    WCHAR* cp = mangle;
    int idx = 0;

    while (*cp != '\0') {
        ASSERT(!inWhiteSpace || !inQuote);
        if (!inQuote && (*cp == ' ' || *cp == '\t')) {
            if (!inWhiteSpace && !inQuote) {
                /* end of token */
                *cp = '\0';
            }
            inWhiteSpace = true;
        } else {
            if (inWhiteSpace) {
                /* start of token */
                if (idx >= *pArgc) {
                    //LOGI("Max #of args (%d) exceeded, ignoring '%ls'",
                    //  *pArgc, cp);
                    break;
                }
                argv[idx++] = cp;
            }

            if (*cp == '"') {
                /* consume the quote; move the last '\0' down too */
                memmove(cp, cp+1, wcslen(cp) * sizeof(WCHAR));
                cp--;
                inQuote = !inQuote;
            }

            inWhiteSpace = false;
        }
        cp++;
    }

    if (inQuote) {
        LOGW("WARNING: ended in quote");
    }

    *pArgc = idx;
}

/*
 * Convert a sub-string to lower case according to rules for English book
 * titles.  Assumes the initial string is in all caps.
 */
static void DowncaseSubstring(CStringA* pStr, int startPos, int endPos,
    bool prevWasSpace)
{
    static const char* shortWords[] = {
        "of", "the", "a", "an", "and", "to", "in"
    };
    static const char* leaveAlone[] = {
        "BBS", "3D"
    };
    static const char* justLikeThis[] = {
        "ProDOS", "IIe", "IIc", "IIgs"
    };
    CStringA token;
    bool firstCap = true;
    int i;

    token = pStr->Mid(startPos, endPos - startPos);
    LOGV("  TOKEN: '%s'", (LPCSTR) token);

    /* these words are left alone */
    for (i = 0; i < NELEM(leaveAlone); i++) {
        if (token.CompareNoCase(leaveAlone[i]) == 0) {
            LOGV("    Leaving alone '%s'", (LPCSTR) token);
            return;
        }
    }

    /* words with specific capitalization */
    for (i = 0; i < NELEM(justLikeThis); i++) {
        if (token.CompareNoCase(justLikeThis[i]) == 0) {
            LOGI("    Setting '%s' to '%s'", (LPCSTR) token, justLikeThis[i]);
            for (int j = startPos; j < endPos; j++)
                pStr->SetAt(j, justLikeThis[i][j - startPos]);
            return;
        }
    }

    /* these words are not capitalized unless they start a phrase */
    if (prevWasSpace) {
        for (i = 0; i < NELEM(shortWords); i++) {
            if (token.CompareNoCase(shortWords[i]) == 0) {
                LOGV("    No leading cap for '%s'", (LPCSTR) token);
                firstCap = false;
                break;
            }
        }
    }

    /* check for roman numerals; we leave those capitalized */
    CString romanTest = token.SpanIncluding("IVX");
    if (romanTest.GetLength() == token.GetLength()) {
        LOGV("    Looks like roman numerals '%s'", (LPCSTR) token);
        return;
    }

    if (firstCap)
        startPos++;

    for (i = startPos; i < endPos; i++) {
        pStr->SetAt(i, tolower(pStr->GetAt(i)));
    }
}

void InjectLowercase(CStringA* pStr)
{
    int len = pStr->GetLength();
    static const char kGapChars[] = " .:&-+/\\()<>@*";
    int startPos, endPos;

    //*pStr = "AND PRODOS FOR THE IIGS";
    //len = pStr->GetLength();
    //LOGI("InjectLowercase: '%ls'", (LPCWSTR) *pStr);

    for (int i = 0; i < len; i++) {
        char ch = pStr->GetAt(i);
        if (ch >= 'a' && ch <= 'z') {
            LOGI("Found lowercase 0x%04x, skipping InjectLower", ch);
            return;
        }
    }

    startPos = 0;
    while (startPos < len) {
        /* find start of token */
        char ch;
        do {
            ch = pStr->GetAt(startPos);
            if (strchr(kGapChars, ch) == NULL)
                break;
            startPos++;
        } while (startPos < len);
        if (startPos == len)
            break;

        /* find end of token */
        endPos = startPos + 1;
        while (endPos < len) {
            ch = pStr->GetAt(endPos);
            if (strchr(kGapChars, ch) != NULL)
                break;
            endPos++;
        }

        /* endPos now points at first char past end of token */

        bool prevWasSpace;
        if (startPos == 0)
            prevWasSpace = false;
        else
            prevWasSpace = (pStr->GetAt(startPos-1) == ' ');
        DowncaseSubstring(pStr, startPos, endPos, prevWasSpace);

        startPos = endPos;
    }
}

bool MatchSemicolonList(const CString set, const CString match)
{
    const WCHAR* cp;
    CString mangle(set);
    int matchLen = match.GetLength();

    if (!matchLen)
        return false;

    mangle.Remove(' ');
    for (cp = mangle; *cp != '\0'; ) {
        if (wcsnicmp(cp, match, matchLen) == 0 &&
            (cp[matchLen] == ';' || cp[matchLen] == '\0'))
        {
            LOGI("+++ Found '%ls' at '%ls'", (LPCWSTR) match, cp);
            return true;
        }

        while (*cp != ';' && *cp != '\0')
            cp++;
        if (*cp == ';')
            cp++;
    }

    LOGD("--- No match for '%ls' in '%ls'", (LPCWSTR) match, (LPCWSTR) set);
    return false;
}

char* StrcpyNew(const char* str)
{
    char* newStr;

    if (str == NULL)
        return NULL;
    newStr = new char[strlen(str)+1];
    if (newStr != NULL)
        strcpy(newStr, str);
    return newStr;
}
