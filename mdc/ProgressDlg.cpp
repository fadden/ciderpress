/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
// ProgressDlg.cpp : implementation file
//

#include "stdafx.h"
#include "mdc.h"
#include "ProgressDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// ProgressDlg dialog

#if 0
ProgressDlg::ProgressDlg(CWnd* pParent /*=NULL*/)
    : CDialog(ProgressDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(ProgressDlg)
        // NOTE: the ClassWizard will add member initialization here
    //}}AFX_DATA_INIT

    fpCancelFlag = NULL;
}
#endif


BEGIN_MESSAGE_MAP(ProgressDlg, CDialog)
    //{{AFX_MSG_MAP(ProgressDlg)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// ProgressDlg message handlers

BOOL ProgressDlg::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
    // TODO: Add your specialized code here and/or call the base class
    
    return CDialog::Create(IDD, pParentWnd);
}

void ProgressDlg::PostNcDestroy() 
{
    // TODO: Add your specialized code here and/or call the base class
    delete this;
    
    //CDialog::PostNcDestroy();
}

/*
 * Update the progress display with the name of the file we're currently
 * working on.
 */
void
ProgressDlg::SetCurrentFile(const WCHAR* fileName)
{
    CWnd* pWnd = GetDlgItem(IDC_PROGRESS_FILENAME);
    ASSERT(pWnd != NULL);
    pWnd->SetWindowText(fileName);
}

void ProgressDlg::OnCancel() 
{
    // TODO: Add extra cleanup here
    LOGI("Cancel button pushed");
    ASSERT(fpCancelFlag != NULL);
    *fpCancelFlag = true;
    
    //CDialog::OnCancel();
}
