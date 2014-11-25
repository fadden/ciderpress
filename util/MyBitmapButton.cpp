/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Code for my buttons with bitmaps.
 */
#include "stdafx.h"
#include "MyBitmapButton.h"


BEGIN_MESSAGE_MAP(MyBitmapButton, CButton)
    ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()


BOOL MyBitmapButton::ReplaceDlgCtrl(CDialog* pDialog, int buttonID)
{
    CWnd* pWnd = pDialog->GetDlgItem(buttonID);
    if (pWnd == NULL)
        return FALSE;

#if 0
    DWORD styles = pWnd->GetStyle();
    //DWORD stylesEx = pWnd->GetExStyle();
    CString caption;
    CRect rect;
    pWnd->GetWindowText(caption);
    pWnd->GetWindowRect(&rect);
    pDialog->ScreenToClient(&rect);

//  pWnd->DestroyWindow();
    if (Create(caption, styles, rect, pDialog, buttonID) == FALSE) {
        LOGI("ERROR: unable to replace dialog ctrl (buttonID=%d)",
            buttonID);
        return FALSE;
    }
#endif

    /* latch on to their window handle */
    Attach(pWnd->m_hWnd);

    return TRUE;
}

BOOL MyBitmapButton::SetBitmapID(int id)
{
    fBitmapID = id;
    UpdateBitmap();
    return TRUE;
}

void MyBitmapButton::UpdateBitmap(void)
{
    HBITMAP hNewBits;

    if (fBitmapID == -1) {
        LOGE("ERROR: UpdateBitmap called before bitmap set");
        ASSERT(false);
        return;
    }

    hNewBits = (HBITMAP) ::LoadImage(AfxGetInstanceHandle(),
        MAKEINTRESOURCE(fBitmapID), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS);
    if (hNewBits == NULL) {
        LOGW("WARNING: LoadImage failed (bitID=%d)", fBitmapID);
        ASSERT(false);
        return;
    }

    ASSERT(GetBitmap() == fhBitmap);

    ::DeleteObject(SetBitmap(hNewBits));
    fhBitmap = hNewBits;
}

void MyBitmapButton::OnSysColorChange(void)
{
    LOGD("MyBitmapButton 0x%p tracking color change", this);
    UpdateBitmap();
}
