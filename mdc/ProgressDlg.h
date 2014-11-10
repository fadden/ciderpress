/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#if !defined(AFX_PROGRESSDLG_H__94B5552B_1337_4FA4_A336_32FA9544ADAC__INCLUDED_)
#define AFX_PROGRESSDLG_H__94B5552B_1337_4FA4_A336_32FA9544ADAC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ProgressDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// ProgressDlg dialog

class ProgressDlg : public CDialog
{
// Construction
public:
    /*ProgressDlg(CWnd* pParent = NULL);   // standard constructor*/

// Dialog Data
    //{{AFX_DATA(ProgressDlg)
    enum { IDD = IDD_PROGRESS };
        // NOTE: the ClassWizard will add data members here
    //}}AFX_DATA

    bool*   fpCancelFlag;

// Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(ProgressDlg)
    public:
    virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
    protected:
    virtual void PostNcDestroy();
    //}}AFX_VIRTUAL

public:
    BOOL Create(CWnd* pParentWnd = NULL) {
        return CDialog::Create(IDD_PROGRESS, pParentWnd);
    }
    void SetCurrentFile(const WCHAR* fileName);

// Implementation
protected:
    // Generated message map functions
    //{{AFX_MSG(ProgressDlg)
    virtual void OnCancel();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROGRESSDLG_H__94B5552B_1337_4FA4_A336_32FA9544ADAC__INCLUDED_)
