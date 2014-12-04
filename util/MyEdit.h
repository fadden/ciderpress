/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * My edit class.
 */
#ifndef UTIL_MYEDIT_H
#define UTIL_MYEDIT_H

/*
 * Replace the edit box in a dialog with this code by calling MyEdit's
 * ReplaceDlgCtrl(this, ID) from the dialog's OnInitDialog.  This class will
 * take over support for that control.
 */
class MyEdit : public CEdit {
public:
    MyEdit(void) {
        fCapsOnly = fHexOnly = fNoWhiteSpace = false;
    }
    virtual ~MyEdit(void) {
        Detach();   // it's not really our window
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

    enum {
        kCapsOnly = 0x01,
        kHexOnly = 0x02,
        kNoWhiteSpace = 0x04,
    };

    virtual void SetProperties(int props);
    virtual BOOL ReplaceDlgCtrl(CDialog* pDialog, int editID);

private:
    DECLARE_COPY_AND_OPEQ(MyEdit)

    virtual BOOL PreTranslateMessage(MSG* pMsg) override;

    bool        fCapsOnly;
    bool        fHexOnly;
    bool        fNoWhiteSpace;

    //DECLARE_MESSAGE_MAP()
};

#endif /*UTIL_MYEDIT_H*/
