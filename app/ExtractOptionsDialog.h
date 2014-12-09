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
#include "HelpTopics.h"

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
    virtual BOOL OnInitDialog(void) override;
    virtual void DoDataExchange(CDataExchange* pDX) override;

    /*
     * Reconfigure controls for best preservation of Apple II formats.
     */
    afx_msg void OnConfigPreserve(void);

    /*
     * Reconfigure controls for easiest viewing under Windows.
     */
    afx_msg void OnConfigConvert(void);

    /*
     * Enable or disable the "Convert high ASCII" button based on the current
     * setting of the radio button above it.
     */
    afx_msg void OnChangeTextConv(void);

    /*
     * They want to choose the folder from a tree.
     */
    afx_msg void OnChooseFolder(void);

    // Context help request (question mark button).
    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo) {
        return MyApp::HandleHelpInfo(lpHelpInfo);
    }

    // User pressed the "Help" button.
    afx_msg void OnHelp(void) {
        MyApp::HandleHelp(this, HELP_TOPIC_EXT_OPTIONS);
    }

    MyBitmapButton  fChooseFolderButton;
    int     fSelectedCount;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_EXTRACTOPTIONSDIALOG_H*/
