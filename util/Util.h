/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Miscellaneous utility classes.
 */
#ifndef __UTIL__
#define __UTIL__

/*
 * Gripper for a resizable window.
 */
class CGripper: public CScrollBar {
protected:
	afx_msg UINT OnNcHitTest(CPoint point);

	DECLARE_MESSAGE_MAP()
};


/*
 * Transfer a buffer of data into a rich edit control.
 */
class RichEditXfer {
public:
	RichEditXfer(const void* buf, long len) : fBuf((char*)buf), fLen(len)
		{}

	static DWORD CALLBACK EditStreamCallback(DWORD dwCookie, LPBYTE pbBuff,
			LONG cb, LONG* pcb);

	const char*	fBuf;
	long		fLen;
};


/*
 * Buffer that expands as data is added to it with stdio-style calls.
 */
class ExpandBuffer {
public:
	ExpandBuffer(long initialSize = 65536) {
		assert(initialSize > 0);
		fInitialSize = initialSize;
		fWorkBuf = nil;
		fWorkCount = fWorkMax = 0;
	}
	virtual ~ExpandBuffer(void) {
		if (fWorkBuf != nil) {
			WMSG0("ExpandBuffer: fWorkBuf not seized; freeing\n");
			delete[] fWorkBuf;
		}
	}
	virtual int CreateWorkBuf(void);

	void Reset(void) {
		delete[] fWorkBuf;
		fWorkBuf = nil;
		fWorkCount = fWorkMax = 0;
	}

	// Copy printf-formatted output into the output buffer.
	void Printf(const char* format, ...);

	// Write binary data to the buffer.
	void Write(const unsigned char* buf, long len);

	// Put a single character in.
	void Putc(char ch);

	// Seize control of the buffer.  It will be the caller's duty to call
	// delete[] to free it.
	virtual void SeizeBuffer(char** ppBuf, long* pLen);
	
protected:
	virtual int GrowWorkBuf(void);

	enum {
		kWorkBufMaxIncrement	= 4*1024*1024,		// limit increase to 4MB jumps
	};

	long		fInitialSize;	// initial size of the work buffer
	char*		fWorkBuf;		// work in progress
	long		fWorkCount;		// quantity of data in buffer
	long		fWorkMax;		// maximum size of buffer
};


/*
 * Windows helpers.
 */
void EnableControl(CDialog* pDlg, int id, bool enable=true);
void MoveControl(CDialog* pDlg, int id, int deltaX, int deltaY,
	bool redraw = true);
void StretchControl(CDialog* pDlg, int id, int deltaX, int deltaY,
	bool redraw = true);
void MoveStretchControl(CDialog* pDlg, int id, int moveX, int moveY,
	int stretchX, int stretchY, bool redraw = true);
HDWP MoveControl(HDWP hdwp, CDialog* pDlg, int id, int deltaX, int deltaY,
	bool redraw = true);
HDWP StretchControl(HDWP hdwp, CDialog* pDlg, int id, int deltaX, int deltaY,
	bool redraw = true);
HDWP MoveStretchControl(HDWP hdwp, CDialog* pDlg, int id, int moveX, int moveY,
	int stretchX, int stretchY, bool redraw = true);
int GetDlgButtonCheck(CWnd* pWnd, int id);
void SetDlgButtonCheck(CWnd* pWnd, int id, int checkVal);
void CreateSimpleFont(CFont* pFont, CWnd* pWnd, const char* typeFace,
		int pointSize);
void GetWin32ErrorString(DWORD err, CString* pStr);
void ShowFailureMsg(CWnd* pWnd, const CString& msg, int titleStrID);
BOOL ShowContextHelp(CWnd* pWnd, HELPINFO* lpHelpInfo);
bool IsWin9x(void);

/*
 * Miscellaneous functions.
 */
int GetPascalString(const char* buf, long maxLen, CString* pStr);
void LogHexDump(const void* buf, long len);
int ComputePercent(LONGLONG part, LONGLONG full);
void FormatDate(time_t when, CString* pStr);
char* stristr(const char* string1, const char* string2);
void VectorizeString(char* mangle, char** argv, int* pArgc);
void InjectLowercase(CString* pStr);
bool MatchSemicolonList(const CString set, const CString match);
char* StrcpyNew(const char* str);

/* time_t values for bad dates */
#define kDateNone		((time_t) -2)
#define kDateInvalid	((time_t) -1)		// should match return from mktime()

#endif /*__UTIL__*/