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


/*
 * Replace a button control in a dialog with ourselves.
 *
 * Returns TRUE on success, FALSE on failure.
 */
BOOL
MyBitmapButton::ReplaceDlgCtrl(CDialog* pDialog, int buttonID)
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

/*
 * Set the bitmap ID, and update the button appropriately.
 */
BOOL
MyBitmapButton::SetBitmapID(int id)
{
    fBitmapID = id;
    UpdateBitmap();
    return TRUE;
}

/*
 * (Re-)load the bitmap and attach it to the button.
 */
void
MyBitmapButton::UpdateBitmap(void)
{
    HBITMAP hNewBits;

    if (fBitmapID == -1) {
        LOGI("ERROR: UpdateBitmap called before bitmap set");
        ASSERT(false);
        return;
    }

    hNewBits = (HBITMAP) ::LoadImage(AfxGetInstanceHandle(),
        MAKEINTRESOURCE(fBitmapID), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS);
    if (hNewBits == NULL) {
        LOGI("WARNING: LoadImage failed (bitID=%d)", fBitmapID);
        ASSERT(false);
        return;
    }

    ASSERT(GetBitmap() == fhBitmap);

    ::DeleteObject(SetBitmap(hNewBits));
    fhBitmap = hNewBits;
}

/*
 * If the system colors have changed, reload the bitmap.
 */
void
MyBitmapButton::OnSysColorChange(void)
{
    LOGI("MyBitmapButton 0x%08lx tracking color change", this);
    UpdateBitmap();
}
