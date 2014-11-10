/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Edit a comment.
 */
#ifndef APP_EDITCOMMENTDIALOG_H
#define APP_EDITCOMMENTDIALOG_H

#include "GenericArchive.h"
#include "resource.h"

/*
 * Edit a comment.  We don't currently put a length limit on the comment
 * field.
 */
class EditCommentDialog : public CDialog {
public:
    EditCommentDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_COMMENT_EDIT, pParentWnd)
    {
        //fComment = "";
        fNewComment = false;
    }
    virtual ~EditCommentDialog(void) {}

    enum { kDeleteCommentID = IDC_COMMENT_DELETE };

    CString     fComment;
    bool        fNewComment;    // entry doesn't already have one

protected:
    // overrides
    virtual BOOL OnInitDialog(void);
    virtual void DoDataExchange(CDataExchange* pDX);

    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo);
    afx_msg void OnHelp(void);
    afx_msg void OnDelete(void);

private:
    DECLARE_MESSAGE_MAP()
};

#endif /*APP_EDITCOMMENTDIALOG_H*/
