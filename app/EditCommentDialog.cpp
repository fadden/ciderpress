/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for EditCommentDialog.
 */
#include "stdafx.h"
#include "EditCommentDialog.h"
#include "HelpTopics.h"

BEGIN_MESSAGE_MAP(EditCommentDialog, CDialog)
    ON_BN_CLICKED(IDC_COMMENT_DELETE, OnDelete)
    ON_WM_HELPINFO()
    ON_COMMAND(IDHELP, OnHelp)
END_MESSAGE_MAP()


BOOL EditCommentDialog::OnInitDialog(void)
{
    /*
     * If this is a new comment, don't show the delete button.
     */
    if (fNewComment) {
        CWnd* pWnd = GetDlgItem(IDC_COMMENT_DELETE);
        pWnd->EnableWindow(FALSE);
    }

    return CDialog::OnInitDialog();
}

void EditCommentDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Text(pDX, IDC_COMMENT_EDIT, fComment);
}

void EditCommentDialog::OnDelete(void)
{
    CString question, title;
    int result;

    title.LoadString(IDS_EDIT_COMMENT);
    question.LoadString(IDS_DEL_COMMENT_OK);
    result = MessageBox(question, title, MB_OKCANCEL | MB_ICONQUESTION);
    if (result == IDCANCEL)
        return;

    EndDialog(kDeleteCommentID);
}

BOOL EditCommentDialog::OnHelpInfo(HELPINFO* lpHelpInfo)
{
    WinHelp((DWORD) lpHelpInfo->iCtrlId, HELP_CONTEXTPOPUP);
    return TRUE;    // yes, we handled it
}

void EditCommentDialog::OnHelp(void)
{
    WinHelp(HELP_TOPIC_EDIT_COMMENT, HELP_CONTEXT);
}
