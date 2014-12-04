/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Replacement spin control with extended range.  Requires auto-buddy.
 *
 * This is based on code originally written by Definitive Solutions, Inc.
 * It was rewritten because their code was miserable.
 */
#ifndef UTIL_MYSPINCTRL_H
#define UTIL_MYSPINCTRL_H

#include <afxcmn.h>     // for CSpinButtonCtrl


// Declare depending on whether this file is in an EXE or a DLL.
class
    #ifdef _WINDLL
    AFX_EXT_CLASS
    #endif
MySpinCtrl : public CSpinButtonCtrl
{
public:
    MySpinCtrl(void) : fLow(0), fHigh(100) {}
    virtual ~MySpinCtrl(void) {}

    // replacements for superclass methods (AFXCMN.H)
    int     SetPos(int nPos);
    int     GetPos() const;
    void    SetRange(int nLower, int nUpper) { SetRange32(nLower, nUpper); }
    void    SetRange32(int nLower, int nUpper);
    DWORD   GetRange() const;       // don't use this
    void    GetRange(int &lower, int& upper) const { GetRange32(lower, upper); }
    void    GetRange32(int &lower, int& upper) const;

protected:
    //virtual void PreSubclassWindow();
    afx_msg void OnDeltaPos(NMHDR* pNMHDR, LRESULT* pResult);

private:
    DECLARE_COPY_AND_OPEQ(MySpinCtrl)

    int fLow, fHigh;

    /*
     * Convert a decimal or hex string to a long.
     *
     * Returns "true" on success, "false" on error.
     */
    bool ConvLong(const WCHAR* str, long* pVal) const;

    DECLARE_MESSAGE_MAP()
};

#endif /*UTIL_MYSPINCTRL_H*/
