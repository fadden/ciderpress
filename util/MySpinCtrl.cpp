/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Replacement spin control.  This one has a 32-bit range and doesn't
 * have the weird wrap-under behavior in hex mode.
 */
#include "stdafx.h"
#include "MySpinCtrl.h"


/*
 * Grab the notification that tells us an update has been made.
 */
BEGIN_MESSAGE_MAP(MySpinCtrl, CSpinButtonCtrl)
    ON_NOTIFY_REFLECT(UDN_DELTAPOS, OnDeltaPos)
END_MESSAGE_MAP()


#if 0
// This is called by MFC after the window's HWND has been bound to the 
// MySpinCtrl object, but before the subclassing occurs.  Note that this class 
// never gets WM_CREATE, because that comes before the subclassing occurs.
void MySpinCtrl::PreSubclassWindow()
{
    _ASSERTE(! (UDS_SETBUDDYINT & GetStyle())  &&  "'Auto Buddy Int' style *MUST NOT* be checked");
    _ASSERTE(  (UDS_AUTOBUDDY   & GetStyle())  &&  "'Auto Buddy' style *MUST* be checked");

    // Do this so that, the first time, the up/down arrows work in the right
    // direction, and the range is a reasonable value.
    CSpinButtonCtrl::SetRange(fLow, fHigh);
    SetRange(fLow, fHigh);
    
    CSpinButtonCtrl::PreSubclassWindow();
}
#endif

bool MySpinCtrl::ConvLong(const WCHAR* str, long* pVal) const
{
    WCHAR* endp;

    *pVal = wcstol(str, &endp, GetBase());
    if (endp == str || *endp != '\0')
        return false;

    return true;
}

/*
 * Handle UDN_DELTAPOS notification.
 */
void MySpinCtrl::OnDeltaPos(NMHDR* pNMHDR, LRESULT* pResult)
{
    _ASSERTE(! (UDS_SETBUDDYINT & GetStyle())  &&  "'Auto Buddy Int' style *MUST* be unchecked");
//  _ASSERTE(  (UDS_AUTOBUDDY   & GetStyle())  &&  "'Auto Buddy' style *MUST* be checked");
    _ASSERTE(  (UDS_NOTHOUSANDS & GetStyle())  &&  "'No Thousands' style *MUST* be checked");

    NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;

    /* grab value from buddy ctrl */
    ASSERT(GetBuddy() != NULL);
    CString buddyStr;
    GetBuddy()->GetWindowText(buddyStr);

    long buddyVal, proposedVal;

    if (!ConvLong(buddyStr, &buddyVal))
        goto bail;      // bad string
    proposedVal = buddyVal - pNMUpDown->iDelta;

    /* peg at the end */
    if (proposedVal < fLow)
        proposedVal = fLow;
    if (proposedVal > fHigh)
        proposedVal = fHigh;

    if (proposedVal != buddyVal) {
        /* set buddy control to new value */
        if (GetBase() == 10)
            buddyStr.Format(L"%d", proposedVal);
        else
            buddyStr.Format(L"%X", proposedVal);
        GetBuddy()->SetWindowText(buddyStr);
    }
    
bail:
    *pResult = 0;
}

/*
 * Set the current position.
 *
 * Returns the previous position.
 */
int MySpinCtrl::SetPos(int nPos)
{
    _ASSERTE(! (UDS_SETBUDDYINT & GetStyle())  &&  "'Auto Buddy Int' style *MUST* be unchecked");
//  _ASSERTE(  (UDS_AUTOBUDDY   & GetStyle())  &&  "'Auto Buddy' style *MUST* be checked");

    CString buddyStr;

    if (nPos < fLow || nPos > fHigh) {
        //LOGI(" MSP setpos out of range");
        return -1;
    }

    if (GetBase() == 10)
        buddyStr.Format(L"%d", nPos);
    else
        buddyStr.Format(L"%X", nPos);

    GetBuddy()->SetWindowText(buddyStr);

    return -1;      // broken
}

/*
 * Get the current position.
 *
 * Returns -1 on error.  Yes, that's bogus, but it's good enough for now.
 */
int MySpinCtrl::GetPos() const
{
    _ASSERTE(! (UDS_SETBUDDYINT & GetStyle())  &&  "'Auto Buddy Int' style *MUST* be unchecked");
//  _ASSERTE(  (UDS_AUTOBUDDY   & GetStyle())  &&  "'Auto Buddy' style *MUST* be checked");

    // Grab buddy edit control value.
    CString buddyStr;
    GetBuddy()->GetWindowText(buddyStr);

    long val;
    if (!ConvLong(buddyStr, &val))
        return -1;

    if (val < fLow || val > fHigh)
        return -1;

    // if they typed a "sloppy value" in, make it look nice
    CString reformatStr;
    if (GetBase() == 10)
        reformatStr.Format(L"%d", val);
    else
        reformatStr.Format(L"%X", val);
    if (buddyStr != reformatStr)
        GetBuddy()->SetWindowText(reformatStr);

    return val;
}

DWORD MySpinCtrl::GetRange(void) const
{
    _ASSERTE(! "Do NOT use this method!");
    return 0;
}

/*
 * Get 32-bit ranges.
 */
void MySpinCtrl::GetRange32(int& lower, int& upper) const
{
    lower = fLow;
    upper = fHigh;
    //LOGI(" MSP getting lower=%d upper=%d", lower, upper);
}

/*
 * Set 32-bit ranges.
 */
void MySpinCtrl::SetRange32(int nLo, int nHi)
{
    _ASSERTE(! (UDS_SETBUDDYINT & GetStyle())  &&  "'Auto Buddy Int' style *MUST* be unchecked");
//  _ASSERTE(  (UDS_AUTOBUDDY   & GetStyle())  &&  "'Auto Buddy' style *MUST* be checked");

    fLow = nLo;
    fHigh = nHi;
    //LOGI(" MSP setting lower=%d upper=%d", fLow, fHigh);
}
