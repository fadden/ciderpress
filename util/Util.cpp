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
 *		CGripper
 * ===========================================================================
 */

/*
 * Fake out the hit testing so that it thinks we're over the scrollable bit
 * when we're over the gripper.
 */
BEGIN_MESSAGE_MAP(CGripper, CScrollBar)
	ON_WM_NCHITTEST()
END_MESSAGE_MAP()

UINT
CGripper::OnNcHitTest(CPoint point) 
{
	UINT ht = CScrollBar::OnNcHitTest(point);
	if (ht == HTCLIENT) {
		ht = HTBOTTOMRIGHT;
	}
	return ht;
}


/*
 * ===========================================================================
 *		RichEditXfer
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
DWORD
RichEditXfer::EditStreamCallback(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG* pcb)
{
	RichEditXfer* pThis = (RichEditXfer*) dwCookie;

	ASSERT(dwCookie != nil);
	ASSERT(pbBuff != nil);
	ASSERT(cb != 0);
	ASSERT(pcb != nil);

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

	//WMSG2("ESC: copyLen=%d, now fLen=%d\n", copyLen, pThis->fLen);

bail:
	*pcb = copyLen;
	return 0;
}


/*
 * ===========================================================================
 *		ExpandBuffer
 * ===========================================================================
 */

/*
 * Allocate the initial buffer.
 */
int
ExpandBuffer::CreateWorkBuf(void)
{
	if (fWorkBuf != nil) {
		ASSERT(fWorkMax > 0);
		return 0;
	}

	assert(fInitialSize > 0);

	fWorkBuf = new char[fInitialSize];
	if (fWorkBuf == nil)
		return -1;

	fWorkCount = 0;
	fWorkMax = fInitialSize;

	return 0;
}

/*
 * Let the caller seize control of our buffer.  We throw away our pointer to
 * the buffer so we don't free it.
 */
void
ExpandBuffer::SeizeBuffer(char** ppBuf, long* pLen)
{
	*ppBuf = fWorkBuf;
	*pLen = fWorkCount;

	fWorkBuf = nil;
	fWorkCount = 0;
	fWorkMax = 0;
}

/*
 * Grow the buffer to the next incremental size.  We keep doubling it until
 * we reach out maximum rate of expansion.
 *
 * Returns 0 on success, -1 on failure.
 */
int
ExpandBuffer::GrowWorkBuf(void)
{
	int newIncr = fWorkMax;
	if (newIncr > kWorkBufMaxIncrement)
		newIncr = kWorkBufMaxIncrement;

	//WMSG3("Extending buffer by %d (count=%d, max=%d)\n",
	//	newIncr, fWorkCount, fWorkMax);

	fWorkMax += newIncr;

	/* debug-only check to catch runaways */
//	ASSERT(fWorkMax < 1024*1024*24);

	char* newBuf = new char[fWorkMax];
	if (newBuf == nil) {
		WMSG1("ALLOC FAILURE (%ld)\n", fWorkMax);
		ASSERT(false);
		fWorkMax -= newIncr;	// put it back so we don't overrun
		return -1;
	}

	memcpy(newBuf, fWorkBuf, fWorkCount);
	delete[] fWorkBuf;
	fWorkBuf = newBuf;

	return 0;
}

/*
 * Write binary data to the buffer.
 */
void
ExpandBuffer::Write(const unsigned char* buf, long len)
{
	if (fWorkBuf == nil)
		CreateWorkBuf();
	while (fWorkCount + len >= fWorkMax) {
		if (GrowWorkBuf() != 0)
			return;
	}
	ASSERT(fWorkCount + len < fWorkMax);
	memcpy(fWorkBuf + fWorkCount, buf, len);
	fWorkCount += len;
}

/*
 * Write one character into the buffer.
 */
void
ExpandBuffer::Putc(char ch)
{
	Write((const unsigned char*) &ch, 1);
}

/*
 * Print a formatted string into the buffer.
 */
void
ExpandBuffer::Printf(const char* format, ...)
{
    va_list args;

    ASSERT(format != nil);

	if (fWorkBuf == nil)
		CreateWorkBuf();

    va_start(args, format);

    if (format != nil) {
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
 *		Windows helpers
 * ===========================================================================
 */

/*
 * Enable or disable a control.
 */
void
EnableControl(CDialog* pDlg, int id, bool enable)
{
	CWnd* pWnd = pDlg->GetDlgItem(id);
	if (pWnd == nil) {
		WMSG1("GLITCH: control %d not found in dialog\n", id);
		ASSERT(false);
	} else {
		pWnd->EnableWindow(enable);
	}
}

/*
 * Move a control so it maintains its same position relative to the bottom
 * and right edges.
 */
void
MoveControl(CDialog* pDlg, int id, int deltaX, int deltaY, bool redraw)
{
	CWnd* pWnd;
	CRect rect;

	pWnd = pDlg->GetDlgItem(id);
	ASSERT(pWnd != nil);
	if (pWnd == nil)
		return;

	pWnd->GetWindowRect(&rect);
	pDlg->ScreenToClient(&rect);
	rect.left += deltaX;
	rect.right += deltaX;
	rect.top += deltaY;
	rect.bottom += deltaY;
	pWnd->MoveWindow(&rect, redraw);
}

/*
 * Make a control larger by the same delta as the parent window.
 */
void
StretchControl(CDialog* pDlg, int id, int deltaX, int deltaY, bool redraw)
{
	CWnd* pWnd;
	CRect rect;

	pWnd = pDlg->GetDlgItem(id);
	ASSERT(pWnd != nil);
	if (pWnd == nil)
		return;

	pWnd->GetWindowRect(&rect);
	pDlg->ScreenToClient(&rect);
	rect.right += deltaX;
	rect.bottom += deltaY;
	pWnd->MoveWindow(&rect, redraw);
}

/*
 * Stretch and move a control.
 */
void
MoveStretchControl(CDialog* pDlg, int id, int moveX, int moveY,
	int stretchX, int stretchY, bool redraw)
{
	MoveControl(pDlg, id, moveX, moveY, redraw);
	StretchControl(pDlg, id, stretchX, stretchY, redraw);
}

/*
 * Move a control so it maintains its same position relative to the bottom
 * and right edges.
 */
HDWP
MoveControl(HDWP hdwp, CDialog* pDlg, int id, int deltaX, int deltaY,
	bool redraw)
{
	CWnd* pWnd;
	CRect rect;

	pWnd = pDlg->GetDlgItem(id);
	ASSERT(pWnd != nil);
	if (pWnd == nil)
		return hdwp;

	pWnd->GetWindowRect(&rect);
	pDlg->ScreenToClient(&rect);
	rect.left += deltaX;
	rect.right += deltaX;
	rect.top += deltaY;
	rect.bottom += deltaY;
	hdwp = DeferWindowPos(hdwp, pWnd->m_hWnd, nil, rect.left, rect.top,
		rect.Width(), rect.Height(), 0);

	return hdwp;
}

/*
 * Make a control larger by the same delta as the parent window.
 */
HDWP
StretchControl(HDWP hdwp, CDialog* pDlg, int id, int deltaX, int deltaY,
	bool redraw)
{
	CWnd* pWnd;
	CRect rect;

	pWnd = pDlg->GetDlgItem(id);
	ASSERT(pWnd != nil);
	if (pWnd == nil)
		return hdwp;

	pWnd->GetWindowRect(&rect);
	pDlg->ScreenToClient(&rect);
	rect.right += deltaX;
	rect.bottom += deltaY;
	hdwp = DeferWindowPos(hdwp, pWnd->m_hWnd, nil, rect.left, rect.top,
		rect.Width(), rect.Height(), 0);

	return hdwp;
}

/*
 * Stretch and move a control.
 */
HDWP
MoveStretchControl(HDWP hdwp, CDialog* pDlg, int id, int moveX, int moveY,
	int stretchX, int stretchY, bool redraw)
{
	CWnd* pWnd;
	CRect rect;

	pWnd = pDlg->GetDlgItem(id);
	ASSERT(pWnd != nil);
	if (pWnd == nil)
		return hdwp;

	pWnd->GetWindowRect(&rect);
	pDlg->ScreenToClient(&rect);
	rect.left += moveX;
	rect.right += moveX;
	rect.top += moveY;
	rect.bottom += moveY;
	rect.right += stretchX;
	rect.bottom += stretchY;
	hdwp = DeferWindowPos(hdwp, pWnd->m_hWnd, nil, rect.left, rect.top,
		rect.Width(), rect.Height(), 0);

	return hdwp;
}

/*
 * Get/set the check state of a button in a dialog.
 */
int
GetDlgButtonCheck(CWnd* pWnd, int id)
{
	CButton* pButton;
	pButton = (CButton*) pWnd->GetDlgItem(id);
	ASSERT(pButton != nil);
	if (pButton == nil)
		return -1;
	return pButton->GetCheck();
}
void
SetDlgButtonCheck(CWnd* pWnd, int id, int checkVal)
{
	CButton* pButton;
	pButton = (CButton*) pWnd->GetDlgItem(id);
	ASSERT(pButton != nil);
	if (pButton == nil)
		return;
	pButton->SetCheck(checkVal);
}


/*
 * Create a font, using defaults for most things.
 */
void
CreateSimpleFont(CFont* pFont, CWnd* pWnd, const char* typeFace,
		int pointSize)
{
	CClientDC dc(pWnd);
	int height;

	height = -((dc.GetDeviceCaps(LOGPIXELSY) * pointSize) / 72);
	pFont->CreateFont(height, 0, 0, 0, FW_NORMAL, 0, 0, 0,
		DEFAULT_CHARSET, OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS,
		DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, typeFace);
}

/*
 * Get a Win32 error string for an error code returned by GetLastError.
 */
void
GetWin32ErrorString(DWORD err, CString* pStr)
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
		WMSG1("FormatMessage on err=0x%08lx failed\n", err);
		pStr->Format("system error 0x%08lx.\n", err);
	} else {
		*pStr = (LPCTSTR)lpMsgBuf;
		//MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
		LocalFree( lpMsgBuf );
	}
}

/*
 * Post a failure message.
 */
void
ShowFailureMsg(CWnd* pWnd, const CString& msg, int titleStrID)
{
	CString failed;

	failed.LoadString(titleStrID);
	pWnd->MessageBox(msg, failed, MB_OK | MB_ICONERROR);
}

/*
 * Show context help, based on the control ID.
 */
BOOL
ShowContextHelp(CWnd* pWnd, HELPINFO* lpHelpInfo)
{
	pWnd->WinHelp((DWORD) lpHelpInfo->iCtrlId, HELP_CONTEXTPOPUP);
	return TRUE;
}

/*
 * Returns "true" if we're running on Win9x (Win95, Win98, WinME), "false"
 * if not (could be WinNT/2K/XP or even Win31 with Win32s).
 */
bool
IsWin9x(void)
{
	OSVERSIONINFO osvers;
	BOOL result;

	osvers.dwOSVersionInfoSize = sizeof(osvers);
	result = ::GetVersionEx(&osvers);
	assert(result != FALSE);

	if (osvers.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
		return true;
	else
		return false;
}


/*
 * ===========================================================================
 *		Miscellaneous
 * ===========================================================================
 */

/*
 * Pull a pascal string out of a buffer and stuff it into "*pStr".
 *
 * Returns the length of the string found, or -1 on error.
 */
int
GetPascalString(const char* buf, long maxLen, CString* pStr)
{
	int len = *buf++;
	int retLen = len;
	*pStr = "";

	if (len > maxLen) {
		WMSG2("Invalid pascal string -- len=%d, maxLen=%d\n", len, maxLen);
		return -1;
	}

	while (len--) {
		if (*buf == '\0') {
			/* this suggests that we're not reading a pascal string */
			WMSG0("Found pascal string with '\\0' in it\n");
			return -1;
		}

		/* not the fastest approach, but it'll do */
		*pStr += *buf++;
	}

	return retLen;
}

/*
 * Dump a block of stuff to the log file.
 */
void
LogHexDump(const void* vbuf, long len)
{
	const unsigned char* buf = (const unsigned char*) vbuf;
	char outBuf[10 + 16*3 +1 +8];	// addr: 00 11 22 ... + 8 bytes slop
	bool skipFirst;
	long addr;
	char* cp = nil;
	int i;

	WMSG2(" Memory at 0x%08lx %ld bytes:\n", buf, len);
	if (len <= 0)
		return;

	addr = (int)buf & ~0x0f;
	if (addr != (int) buf) {
		sprintf(outBuf, "%08lx: ", addr);
		for (i = addr; i < (int) buf; i++)
			strcat(outBuf, "   ");
		cp = outBuf + strlen(outBuf);
		skipFirst = false;
	} else {
		skipFirst = true;
	}

	while (len--) {
		if (((int) buf & 0x0f) == 0) {
			/* output the old, start a new line */
			if (skipFirst) {
				skipFirst = false;
			} else {
				WMSG1("  %s\n", outBuf);
				addr += 16;
			}
			sprintf(outBuf, "%08lx: ", addr);
			cp = outBuf + strlen(outBuf);
		}

		sprintf(cp, "%02x ", *buf++);
		cp += 3;
	}

	/* output whatever is left */
	WMSG1("  %s\n", outBuf);
}

/*
 * Compute a percentage.
 */
int
ComputePercent(LONGLONG part, LONGLONG full)
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

/*
 * Format a time_t into a string.
 *
 * (Should take format as an argument, so we can use global format set by
 * user preferences.)
 */
void
FormatDate(time_t when, CString* pStr)
{
	if (when == kDateNone) {
		*pStr = "[No Date]";
	} else if (when == kDateInvalid) {
		*pStr = "<invalid>";
	} else {
		CTime modWhen(when);
		*pStr = modWhen.Format("%d-%b-%y %H:%M");
	}
}

/*
 * Case-insensitive version of strstr(), pulled from the MSDN stuff that
 * comes with VC++6.0.
 *
 * The isalpha() stuff is an optimization, so they can skip the tolower()
 * in the outer loop comparison.
 */
char*
stristr(const char* string1, const char* string2)
{
	char *cp1 = (char*) string1, *cp2, *cp1a;
	char first; 			 // get the first char in string to find

	first = string2[0]; 	 // first char often won't be alpha
	if (isalpha(first)) 	{
		first = tolower(first);
		for ( ; *cp1  != '\0'; cp1++)
		{
			if (tolower(*cp1) == first)
			{
				for (cp1a = &cp1[1], cp2 = (char*) &string2[1]; ;
				cp1a++, cp2++)
				{ 
					if (*cp2 == '\0')
						return cp1;
					if (tolower(*cp1a) != tolower(*cp2))
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
				for (cp1a = &cp1[1], cp2 = (char*) &string2[1]; ;
				cp1a++, cp2++)
				{
					if (*cp2 == '\0')
						return cp1;
					if (tolower(*cp1a) != tolower(*cp2))
						break;
				}
			}
		}
	}
	return NULL;
}


/*
 * Break a string down into its component parts.
 *
 * "mangle" will be mangled (various bits stomped by '\0'), "argv" will
 * receive pointers to the strings, and "*pArgc" will hold the number of
 * arguments in the vector.  The initial value of "*pArgc" should hold the
 * maximum "argv" capacity (including program name in argv[0]).
 *
 * The argv pointers will point into "mangle"; no new storage will be
 * allocated.
 */
void
VectorizeString(char* mangle, char** argv, int* pArgc)
{
	bool inWhiteSpace = true;
	bool inQuote = false;
	char* cp = mangle;
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
					//WMSG2("Max #of args (%d) exceeded, ignoring '%s'\n",
					//	*pArgc, cp);
					break;
				}
				argv[idx++] = cp;
			}

			if (*cp == '"') {
				/* consume the quote */
				memmove(cp, cp+1, strlen(cp));	// move the last '\0' down too
				cp--;
				inQuote = !inQuote;
			}

			inWhiteSpace = false;
		}
		cp++;
	}

	if (inQuote) {
		WMSG0("WARNING: ended in quote\n");
	}

	*pArgc = idx;
}


/*
 * Convert a sub-string to lower case according to rules for English book
 * titles.  Assumes the initial string is in all caps.
 */
static void
DowncaseSubstring(CString* pStr, int startPos, int endPos,
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
	CString token;
	bool firstCap = true;
	int i;

	token = pStr->Mid(startPos, endPos - startPos);
	//WMSG1("  TOKEN: '%s'\n", token);

	/* these words are left alone */
	for (i = 0; i < NELEM(leaveAlone); i++) {
		if (token.CompareNoCase(leaveAlone[i]) == 0) {
			//WMSG1("    Leaving alone '%s'\n", token);
			return;
		}
	}

	/* words with specific capitalization */
	for (i = 0; i < NELEM(justLikeThis); i++) {
		if (token.CompareNoCase(justLikeThis[i]) == 0) {
			WMSG2("    Setting '%s' to '%s'\n", token, justLikeThis[i]);
			for (int j = startPos; j < endPos; j++)
				pStr->SetAt(j, justLikeThis[i][j - startPos]);
			return;
		}
	}

	/* these words are not capitalized unless they start a phrase */
	if (prevWasSpace) {
		for (i = 0; i < NELEM(shortWords); i++) {
			if (token.CompareNoCase(shortWords[i]) == 0) {
				//WMSG1("    No leading cap for '%s'\n", token);
				firstCap = false;
				break;
			}
		}
	}

	/* check for roman numerals; we leave those capitalized */
	CString romanTest = token.SpanIncluding("IVX");
	if (romanTest.GetLength() == token.GetLength()) {
		//WMSG1("    Looks like roman numerals '%s'\n", token);
		return;
	}

	if (firstCap)
		startPos++;

	for (i = startPos; i < endPos; i++) {
		pStr->SetAt(i, tolower(pStr->GetAt(i)));
	}
}

/*
 * Convert parts of the filename to lower case.
 *
 * If the name already has lowercase characters, do nothing.
 */
void
InjectLowercase(CString* pStr)
{
	int len = pStr->GetLength();
	static const char* kGapChars = " .:&-+/\\()<>@*";
	int startPos, endPos;

	//*pStr = "AND PRODOS FOR THE IIGS";
	//len = pStr->GetLength();
	//WMSG1("InjectLowercase: '%s'\n", *pStr);

	for (int i = 0; i < len; i++) {
		char ch = pStr->GetAt(i);
		if (ch >= 'a' && ch <= 'z') {
			WMSG1("Found lowercase 0x%02x, skipping InjectLower\n", ch);
			return;
		}
	}

	startPos = 0;
	while (startPos < len) {
		/* find start of token */
		char ch;
		do {
			ch = pStr->GetAt(startPos);
			if (strchr(kGapChars, ch) == nil)
				break;
			startPos++;
		} while (startPos < len);
		if (startPos == len)
			break;

		/* find end of token */
		endPos = startPos + 1;
		while (endPos < len) {
			ch = pStr->GetAt(endPos);
			if (strchr(kGapChars, ch) != nil)
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


/*
 * Test to see if a sub-string matches a value in a set of strings.  The set
 * comes from a semicolon-delimited string.
 */
bool
MatchSemicolonList(const CString set, const CString match)
{
	const char* cp;
	CString mangle(set);
	int matchLen = match.GetLength();

	if (!matchLen)
		return false;

	mangle.Remove(' ');
	for (cp = mangle; *cp != '\0'; ) {
		if (strncasecmp(cp, match, matchLen) == 0 &&
			(cp[matchLen] == ';' || cp[matchLen] == '\0'))
		{
			WMSG2("+++ Found '%s' at '%s'\n", (LPCTSTR) match, cp);
			return true;
		}

		while (*cp != ';' && *cp != '\0')
			cp++;
		if (*cp == ';')
			cp++;
	}

	WMSG2("--- No match for '%s' in '%s'\n", (LPCTSTR) match, (LPCTSTR) set);
	return false;
}


/*
 * Like strcpy(), but allocate with new[] instead.
 *
 * If "str" is nil, or "new" fails, this returns nil.
 */
char*
StrcpyNew(const char* str)
{
	char* newStr;

	if (str == nil)
		return nil;
	newStr = new char[strlen(str)+1];
	if (newStr != nil)
		strcpy(newStr, str);
	return newStr;
}
