/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Trivial implementation of EOLScanDialog.
 *
 * I'd stuff the whole thing in the header, but I need the "help" button to
 * work, and it's easiest to do that through a message map.
 */
#include "StdAfx.h"
#include "EOLScanDialog.h"
#include "HelpTopics.h"

BEGIN_MESSAGE_MAP(EOLScanDialog, CDialog)
    ON_COMMAND(IDHELP, OnHelp)
END_MESSAGE_MAP()

/*
 * Fill in the blanks.
 */
BOOL
EOLScanDialog::OnInitDialog(void)
{
    CWnd* pWnd;
    CString fmt;

    fmt.Format("%ld", fCountChars);
    pWnd = GetDlgItem(IDC_EOLSCAN_CHARS);
    pWnd->SetWindowText(fmt);

    fmt.Format("%ld", fCountCR);
    pWnd = GetDlgItem(IDC_EOLSCAN_CR);
    pWnd->SetWindowText(fmt);

    fmt.Format("%ld", fCountLF);
    pWnd = GetDlgItem(IDC_EOLSCAN_LF);
    pWnd->SetWindowText(fmt);

    fmt.Format("%ld", fCountCRLF);
    pWnd = GetDlgItem(IDC_EOLSCAN_CRLF);
    pWnd->SetWindowText(fmt);

    fmt.Format("%ld", fCountHighASCII);
    pWnd = GetDlgItem(IDC_EOLSCAN_HIGHASCII);
    pWnd->SetWindowText(fmt);

    return CDialog::OnInitDialog();
}

/*
 * User pressed the "Help" button.
 */
void
EOLScanDialog::OnHelp(void)
{
    WinHelp(HELP_TOPIC_EOL_SCAN, HELP_CONTEXT);
}
