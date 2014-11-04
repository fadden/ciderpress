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
#ifndef __MYBITMAPBUTTON__
#define __MYBITMAPBUTTON__

class MyBitmapButton : public CButton {
public:
    MyBitmapButton(void) {
        fhBitmap = nil;
        fBitmapID = -1;
    }
    virtual ~MyBitmapButton(void) {
        //WMSG0("~MyBitmapButton()\n");
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

    virtual BOOL SetBitmapID(int id);
    virtual BOOL ReplaceDlgCtrl(CDialog* pDialog, int buttonID);

protected:
    virtual void UpdateBitmap(void);
    afx_msg void OnSysColorChange(void);

private:
    HBITMAP     fhBitmap;
    int         fBitmapID;

    DECLARE_MESSAGE_MAP()
};

#endif /*__MYBITMAPBUTTON__*/