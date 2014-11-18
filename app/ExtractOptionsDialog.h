/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Choose options related to file extraction.
 */
#ifndef APP_EXTRACTOPTIONSDIALOG_H
#define APP_EXTRACTOPTIONSDIALOG_H

#include "../util/UtilLib.h"
#include "resource.h"

/*
 * Our somewhat complicated extraction options dialog.
 */
class ExtractOptionsDialog : public CDialog {
public:
    ExtractOptionsDialog(int selCount, CWnd* pParentWnd = NULL) :
        CDialog(IDD_EXTRACT_FILES, pParentWnd), fSelectedCount(selCount)
    {
        // init values; these should be overridden before DoModal
        fExtractPath = "";
        fFilesToExtract = 0;
        fConvEOL = 0;
        fConvHighASCII = FALSE;
        fIncludeDataForks = fIncludeRsrcForks = fIncludeDiskImages = FALSE;
        fEnableReformat = fDiskTo2MG = FALSE;
        fAddTypePreservation = fAddExtension = fStripFolderNames = FALSE;
        fOverwriteExisting = FALSE;
    }
    virtual ~ExtractOptionsDialog(void) {
        //LOGI("~ExtractOptionsDialog()");
    }

    CString fExtractPath;

    enum { kExtractSelection = 0, kExtractAll = 1 };
    int     fFilesToExtract;

//  enum { kPreserveNone = 0, kPreserveTypes, kPreserveAndExtend };
//  int     fTypePreservation;

    // this must match tab order of radio buttons in dialog
    enum { kConvEOLNone = 0, kConvEOLType, kConvEOLAuto, kConvEOLAll };
    int     fConvEOL;
    BOOL    fConvHighASCII;

//  enum { kDiskImageNoExtract = 0, kDiskImageAsPO = 1, kDiskImageAs2MG };
//  int     fDiskImageExtract;

    BOOL    fIncludeDataForks;
    BOOL    fIncludeRsrcForks;
    BOOL    fIncludeDiskImages;

    BOOL    fEnableReformat;
    BOOL    fDiskTo2MG;

    BOOL    fAddTypePreservation;
    BOOL    fAddExtension;
    BOOL    fStripFolderNames;

    BOOL    fOverwriteExisting;

    bool ShouldTryReformat(void) const {
        return fEnableReformat != 0;
    }

private:
    virtual BOOL OnInitDialog(void);
    virtual void DoDataExchange(CDataExchange* pDX);

    afx_msg void OnConfigPreserve(void);
    afx_msg void OnConfigConvert(void);
    afx_msg void OnChangeTextConv(void);
    afx_msg void OnChooseFolder(void);
    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo);
    afx_msg void OnHelp(void);

    MyBitmapButton  fChooseFolderButton;
    int     fSelectedCount;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_EXTRACTOPTIONSDIALOG_H*/
