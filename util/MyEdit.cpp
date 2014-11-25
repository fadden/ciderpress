/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for MyEdit class.
 */
#include "StdAfx.h"
#include "MyEdit.h"

//BEGIN_MESSAGE_MAP(MyBitmapButton, CButton)
//  ON_WM_SYSCOLORCHANGE()
//END_MESSAGE_MAP()

/*
 * Replace an edit control in a dialog with ourselves.
 *
 * Returns TRUE on success, FALSE on failure.
 */
BOOL MyEdit::ReplaceDlgCtrl(CDialog* pDialog, int editID)
{
    CWnd* pWnd = pDialog->GetDlgItem(editID);
    if (pWnd == NULL)
        return FALSE;

    /* latch on to their window handle */
    Attach(pWnd->m_hWnd);

    return TRUE;
}

/*
 * Set the properties that make us special.
 */
void MyEdit::SetProperties(int props)
{
    fCapsOnly = (props & kCapsOnly) != 0;
    fHexOnly = (props & kHexOnly) != 0;
    fNoWhiteSpace = (props & kNoWhiteSpace) != 0;
}

/*
 * Special keypress handling.
 */
BOOL MyEdit::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_CHAR) {
        if (fCapsOnly)
            pMsg->wParam = toupper(pMsg->wParam);
        if (fNoWhiteSpace) {
            if (pMsg->wParam == ' ' || pMsg->wParam == '\t')
                return TRUE;        // we handled it
        }
        if (fHexOnly) {
            if ((pMsg->wParam >= '0' && pMsg->wParam <= '9') ||
                (pMsg->wParam >= 'a' && pMsg->wParam <= 'f') ||
                (pMsg->wParam >= 'A' && pMsg->wParam <= 'F'))
            {
                /* good, keep going */
            } else if ((pMsg->wParam >= 0x20 && pMsg->wParam < 0x7f) ||
                (pMsg->wParam >= 0xa0 && pMsg->wParam <= 0xff))
            {
                /* ignore this character */
                return TRUE;    // we handled it
            }
            /* else it's a backspace or DEL or something */
        }
    }

    return CEdit::PreTranslateMessage(pMsg);
}
