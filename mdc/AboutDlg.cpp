/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
// AboutDlg.cpp : implementation file
//

#include "stdafx.h"
#include "mdc.h"
#include "AboutDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// AboutDlg dialog


AboutDlg::AboutDlg(CWnd* pParent /*=NULL*/)
    : CDialog(AboutDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(AboutDlg)
        // NOTE: the ClassWizard will add member initialization here
    //}}AFX_DATA_INIT
}


BEGIN_MESSAGE_MAP(AboutDlg, CDialog)
    //{{AFX_MSG_MAP(AboutDlg)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// AboutDlg message handlers

BOOL AboutDlg::OnInitDialog() 
{
    CDialog::OnInitDialog();
    
    // TODO: Add extra initialization here
    CWnd* pWnd = GetDlgItem(IDC_ABOUT_VERS);
    ASSERT(pWnd != nil);
    CString fmt, newText;

    pWnd->GetWindowText(fmt);
    newText.Format(fmt, kAppMajorVersion, kAppMinorVersion, kAppBugVersion);
    pWnd->SetWindowText(newText);
    WMSG1("STR is '%ls'\n", (LPCWSTR) newText);

    return TRUE;  // return TRUE unless you set the focus to a control
                  // EXCEPTION: OCX Property Pages should return FALSE
}
