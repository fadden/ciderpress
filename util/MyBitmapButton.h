/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * My BitmapButton class.
 *
 * An extension of CBitmap that updates the button's bitmap automatically when
 * the system colors change.  Also handles installing itself into a dialog.
 */
#ifndef UTIL_MYBITMAPBUTTON_H
#define UTIL_MYBITMAPBUTTON_H

class MyBitmapButton : public CButton {
public:
    MyBitmapButton(void) {
        fhBitmap = NULL;
        fBitmapID = -1;
    }
    virtual ~MyBitmapButton(void) {
        //LOGI("~MyBitmapButton()");
        Detach();   // it's not really our window
        ::DeleteObject(fhBitmap);
    }

    /* don't allow creation of a window */
    int Create(LPCTSTR, LPCTSTR, DWORD, const RECT&, CWnd*, UINT, CCreateContext*) {
        ASSERT(false);
        return FALSE;
    }
    int Create(LPCTSTR, DWORD, const RECT&, CWnd*, UINT) {
        ASSERT(false);
        return FALSE;
    }

    /*
     * Replace a button control in a dialog with ourselves.
     *
     * Returns TRUE on success, FALSE on failure.
     */
    virtual BOOL ReplaceDlgCtrl(CDialog* pDialog, int buttonID);

    /*
     * Set the bitmap ID, and update the button appropriately.
     */
    virtual BOOL SetBitmapID(int id);

protected:
    /*
     * If the system colors have changed, reload the bitmap.
     */
    afx_msg void OnSysColorChange(void);

    /*
     * (Re-)load the bitmap and attach it to the button.
     */
    virtual void UpdateBitmap(void);

private:
    DECLARE_COPY_AND_OPEQ(MyBitmapButton)

    HBITMAP     fhBitmap;
    int         fBitmapID;

    DECLARE_MESSAGE_MAP()
};

#endif /*UTIL_MYBITMAPBUTTON_H*/
