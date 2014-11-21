/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Options for recompressing files.  This is derived from the "use selection"
 * dialog.
 */
#ifndef APP_RECOMPESSOPTIONSDIALOG_H
#define APP_RECOMPESSOPTIONSDIALOG_H

#include "UseSelectionDialog.h"
#include "../nufxlib/NufxLib.h"
#include "resource.h"

/*
 * Straightforward confirmation plus a drop-list.
 */
class RecompressOptionsDialog : public UseSelectionDialog {
public:
    RecompressOptionsDialog(int selCount, CWnd* pParentWnd = NULL) :
        UseSelectionDialog(selCount, pParentWnd, IDD_RECOMPRESS_OPTS)
    {
        fCompressionType = 0;
    }
    virtual ~RecompressOptionsDialog(void) {}

    // maps directly to NuThreadFormat enum
    int     fCompressionType;

private:
    virtual BOOL OnInitDialog(void) override;
    virtual void DoDataExchange(CDataExchange* pDX) override;

    /*
     * Load strings into the combo box.  Only load formats supported by the
     * NufxLib DLL.
     *
     * Returns the combo box index for the format matching "fmt".
     */
    int LoadComboBox(NuThreadFormat fmt);

    int     fCompressionIdx;        // drop list index

    //DECLARE_MESSAGE_MAP()
};

#endif /*APP_RECOMPESSOPTIONSDIALOG_H*/
