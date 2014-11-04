/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Options for recompressing files.  This is derived from the "use selection"
 * dialog.
 */
#ifndef __RECOMPRESS_OPTIONS_DIALOG__
#define __RECOMPRESS_OPTIONS_DIALOG__

#include "UseSelectionDialog.h"
#include "../prebuilt/NufxLib.h"
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
    virtual BOOL OnInitDialog(void);
    virtual void DoDataExchange(CDataExchange* pDX);

    int LoadComboBox(NuThreadFormat fmt);

    int     fCompressionIdx;        // drop list index

    //DECLARE_MESSAGE_MAP()
};

#endif /*__RECOMPRESS_OPTIONS_DIALOG__*/