/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#if !defined(AFX_ABOUTDLG_H__340C7108_C3D5_4F24_98AF_C1C614460F6A__INCLUDED_)
#define AFX_ABOUTDLG_H__340C7108_C3D5_4F24_98AF_C1C614460F6A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// AboutDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// AboutDlg dialog

class AboutDlg : public CDialog
{
// Construction
public:
    AboutDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
    //{{AFX_DATA(AboutDlg)
    enum { IDD = IDD_ABOUTBOX };
        // NOTE: the ClassWizard will add data members here
    //}}AFX_DATA


// Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(AboutDlg)
    //}}AFX_VIRTUAL

// Implementation
protected:

    // Generated message map functions
    //{{AFX_MSG(AboutDlg)
    virtual BOOL OnInitDialog();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ABOUTDLG_H__340C7108_C3D5_4F24_98AF_C1C614460F6A__INCLUDED_)
