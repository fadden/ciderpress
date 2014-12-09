/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Disk Edit "open file" dialog.
 *
 * If the dialog returns with IDOK, "fName" will be a string at least 1
 * character long.
 *
 * Currently this is a trivial edit box that asks for a name.  In the future
 * this might present a list or tree view to choose from.
 *
 * NOTE: we probably want to have a read-only flag here, defaulted to the
 * state of the sector editor.  The read-only state of the underlying FS
 * doesn't matter, since we're writing sectors, not really editing files.
 */
#ifndef APP_DEFILEDIALOG_H
#define APP_DEFILEDIALOG_H

#include "resource.h"
#include "../diskimg/DiskImg.h"

using namespace DiskImgLib;


/*
 * Class declaration.  Nothing special.
 */
class DEFileDialog : public CDialog {
public:
    DEFileDialog(CWnd* pParentWnd = NULL) : CDialog(IDD_DEFILE, pParentWnd)
    {
        fOpenRsrcFork = false;
        fName = L"";
    }
    virtual ~DEFileDialog(void) {}

    void Setup(DiskFS* pDiskFS) {
        fpDiskFS = pDiskFS;
    }

    CString     fName;
    int         fOpenRsrcFork;

protected:
    /*
     * Turn off the "OK" button, which is only active when some text
     * has been typed in the window.
     */
    virtual BOOL OnInitDialog(void) override;

    /*
     * Get the filename and the "open resource fork" check box.
     */
    virtual void DoDataExchange(CDataExchange* pDX) override;

    /*
     * The text has changed.  If there's nothing in the box, dim the
     * "OK" button.
     */
    afx_msg virtual void OnChange(void);

    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo) {
        return MyApp::HandleHelpInfo(lpHelpInfo);
    }

private:
    DiskFS*     fpDiskFS;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_DEFILEDIALOG_H*/
