/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Miscellaneous utility classes.
 */
#ifndef UTIL_UTIL_H
#define UTIL_UTIL_H

#include <sal.h>

/*
 * Gripper for a resizable window.
 */
class CGripper: public CScrollBar {
protected:
    afx_msg LRESULT OnNcHitTest(CPoint point);

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

    const char* fBuf;
    long        fLen;

private:
    DECLARE_COPY_AND_OPEQ(RichEditXfer)
};


/*
 * Buffer that expands as data is added to it with stdio-style calls.
 */
class ExpandBuffer {
public:
    ExpandBuffer(long initialSize = 65536) {
        ASSERT(initialSize > 0);
        fInitialSize = initialSize;
        fWorkBuf = NULL;
        fWorkCount = fWorkMax = 0;
    }
    virtual ~ExpandBuffer(void) {
        if (fWorkBuf != NULL) {
            LOGI("ExpandBuffer: fWorkBuf not seized; freeing");
            delete[] fWorkBuf;
        }
    }

    /*
     * Allocate the initial buffer.
     */
    virtual int CreateWorkBuf(void);

    void Reset(void) {
        delete[] fWorkBuf;
        fWorkBuf = NULL;
        fWorkCount = fWorkMax = 0;
    }

    // Copy printf-formatted output into the output buffer.
    void Printf(_Printf_format_string_ const char* format, ...);

    // Write binary data to the buffer.
    void Write(const unsigned char* buf, long len);

    // Put a single character in.
    void Putc(char ch);

    // Seize control of the buffer.  It will be the caller's duty to call
    // delete[] to free it.
    virtual void SeizeBuffer(char** ppBuf, long* pLen);
    
protected:
    /*
     * Grow the buffer to the next incremental size.  We keep doubling it until
     * we reach out maximum rate of expansion.
     *
     * Returns 0 on success, -1 on failure.
     */
    virtual int GrowWorkBuf(void);

    enum {
        kWorkBufMaxIncrement    = 4*1024*1024,      // limit increase to 4MB jumps
    };

    long        fInitialSize;   // initial size of the work buffer
    char*       fWorkBuf;       // work in progress
    long        fWorkCount;     // quantity of data in buffer
    long        fWorkMax;       // maximum size of buffer

private:
    DECLARE_COPY_AND_OPEQ(ExpandBuffer)
};


/*
 * ====================================
 *      Windows helpers
 * ====================================
 */

/*
 * Enable or disable a control.
 */
void EnableControl(CDialog* pDlg, int id, bool enable=true);

/*
 * Move a control so it maintains its same position relative to the bottom
 * and right edges.
 */
void MoveControl(CDialog* pDlg, int id, int deltaX, int deltaY,
    bool redraw = true);

/*
 * Make a control larger by the same delta as the parent window.
 */
void StretchControl(CDialog* pDlg, int id, int deltaX, int deltaY,
    bool redraw = true);

/*
 * Stretch and move a control.
 */
void MoveStretchControl(CDialog* pDlg, int id, int moveX, int moveY,
    int stretchX, int stretchY, bool redraw = true);

/*
 * Move a control so it maintains its same position relative to the bottom
 * and right edges.
 */
HDWP MoveControl(HDWP hdwp, CDialog* pDlg, int id, int deltaX, int deltaY,
    bool redraw = true);

/*
 * Make a control larger by the same delta as the parent window.
 */
HDWP StretchControl(HDWP hdwp, CDialog* pDlg, int id, int deltaX, int deltaY,
    bool redraw = true);

/*
 * Stretch and move a control.
 */
HDWP MoveStretchControl(HDWP hdwp, CDialog* pDlg, int id, int moveX, int moveY,
    int stretchX, int stretchY, bool redraw = true);

/*
 * Get the check state of a button in a dialog.
 */
int GetDlgButtonCheck(CWnd* pWnd, int id);

/*
 * Set the check state of a button in a dialog.
 */
void SetDlgButtonCheck(CWnd* pWnd, int id, int checkVal);

/*
 * Create a font, using defaults for most things.
 */
void CreateSimpleFont(CFont* pFont, CWnd* pWnd, const WCHAR* typeFace,
        int pointSize);

/*
 * Get a Win32 error string for an error code returned by GetLastError.
 */
void GetWin32ErrorString(DWORD err, CString* pStr);

/*
 * Post a failure message in a message box.
 */
void ShowFailureMsg(CWnd* pWnd, const CString& msg, int titleStrID);

/*
 * Post a failure message in a message box.
 */
void ShowFailureMsg(CWnd* pWnd, int msgId, int titleStrID);


/*
 * Returns "true" if we're running on Win9x (Win95, Win98, WinME), "false"
 * if not (could be WinNT/2K/XP or even Win31 with Win32s).
 */
bool IsWin9x(void);

/*
 * Wrapper for CString::LoadString that checks the return value and logs a
 * complaint if the load fails.
 */
void CheckedLoadString(CString* pString, UINT nID);

/*
 * ====================================
 *      Miscellaneous functions
 * ====================================
 */

/*
 * Pull a pascal string out of a buffer and stuff it into "*pStr".
 *
 * Returns the length of the string found, or -1 on error.
 */
int GetPascalString(const uint8_t* buf, long maxLen, CString* pStr);

/*
 * Dump a block of stuff to the log file.
 */
void LogHexDump(const void* buf, long len);

/*
 * Compute a percentage.
 */
int ComputePercent(LONGLONG part, LONGLONG full);

/*
 * Format a time_t into a string.
 *
 * (Should take format as an argument, so we can use global format set by
 * user preferences.)
 */
void FormatDate(time_t when, CString* pStr);

/*
 * Case-insensitive version of strstr(), pulled from the MSDN stuff that
 * comes with VC++6.0.
 *
 * The isalpha() stuff is an optimization, so they can skip the tolower()
 * in the outer loop comparison.
 */
const WCHAR* Stristr(const WCHAR* string1, const WCHAR* string2);

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
void VectorizeString(WCHAR* mangle, WCHAR** argv, int* pArgc);

/*
 * Convert parts of the Mac OS Roman filename to lower case.
 *
 * If the name already has lowercase characters, do nothing.
 */
void InjectLowercase(CStringA* pStr);

/*
 * Test to see if a sub-string matches a value in a set of strings.  The set
 * comes from a semicolon-delimited string.
 */
bool MatchSemicolonList(const CString set, const CString match);

/*
 * Like strcpy(), but allocate with new[] instead.
 *
 * If "str" is NULL, or "new" fails, this returns NULL.
 *
 * TODO: this should be "StrdupNew()".
 */
char* StrcpyNew(const char* str);

/* time_t values for bad dates */
#define kDateNone       ((time_t) -2)
#define kDateInvalid    ((time_t) -1)       // should match return from mktime()

#endif /*UTIL_UTIL_H*/
