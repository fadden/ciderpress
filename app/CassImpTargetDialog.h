/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Choose file name and characteristics for a file imported from an audio
 * cassette tape.
 */
#ifndef APP_CASSIMPTARGETDIALOG_H
#define APP_CASSIMPTARGETDIALOG_H

#include "resource.h"

/*
 * Get a filename, allow them to override the file type, and get a hexadecimal
 * start address for binary files.
 */
class CassImpTargetDialog : public CDialog {
public:
    CassImpTargetDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_CASSIMPTARGET, pParentWnd),
        fStartAddr(0x0800),
        fFileTypeIndex(0)
        {}
    virtual ~CassImpTargetDialog(void) {}

    long GetFileType(void) const;
    void SetFileType(long type);

    CString fFileName;
    unsigned short  fStartAddr;     // start addr for BIN files
    long    fFileLength;            // used for BIN display

private:
    virtual BOOL OnInitDialog(void);
    virtual void DoDataExchange(CDataExchange* pDX);
    afx_msg void OnTypeChange(void);
    afx_msg void OnAddrChange(void);

    MyEdit  fAddrEdit;              // replacement edit ctrl for addr field

    long GetStartAddr(void) const;

    /* for radio button; enum must match order of controls in dialog */
    enum { kTypeBAS = 0, kTypeINT, kTypeBIN };
    int     fFileTypeIndex;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_CASSIMPTARGETDIALOG_H*/
