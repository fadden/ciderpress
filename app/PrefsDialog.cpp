/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation classes for the preferences property sheet
 */
#include "stdafx.h"
#include "PrefsDialog.h"
#include "ChooseDirDialog.h"
#include "EditAssocDialog.h"
#include "Main.h"
#include "NufxArchive.h"
#include "HelpTopics.h"
#include "resource.h"
#include <afxpriv.h>        // need WM_COMMANDHELP


/*
 * ===========================================================================
 *      PrefsGeneralPage
 * ===========================================================================
 */

BEGIN_MESSAGE_MAP(PrefsGeneralPage, CPropertyPage)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_COL_PATHNAME, IDC_COL_ACCESS, OnChangeRange)
    ON_BN_CLICKED(IDC_PREF_SHRINKIT_COMPAT, OnChange)
    ON_BN_CLICKED(IDC_PREF_REDUCE_SHK_ERROR_CHECKS, OnChange)
    ON_BN_CLICKED(IDC_PREF_SHK_BAD_MAC, OnChange)
    ON_BN_CLICKED(IDC_PREF_COERCE_DOS, OnChange)
    ON_BN_CLICKED(IDC_PREF_SPACES_TO_UNDER, OnChange)
    ON_BN_CLICKED(IDC_PREF_PASTE_JUNKPATHS, OnChange)
    ON_BN_CLICKED(IDC_PREF_SUCCESS_BEEP, OnChange)
    ON_BN_CLICKED(IDC_COL_DEFAULTS, OnDefaults)
    ON_BN_CLICKED(IDC_PREF_ASSOCIATIONS, OnAssociations)
    ON_MESSAGE(WM_HELP, OnHelp)
    ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
END_MESSAGE_MAP()


/*
 * If they clicked on a checkbox, just mark the page as dirty so the "apply"
 * button will be enabled.
 */
void
PrefsGeneralPage::OnChange(void)
{
    SetModified(TRUE);
}
void
PrefsGeneralPage::OnChangeRange(UINT nID)
{
    SetModified(TRUE);
}

/*
 * Handle a click of the "defaults" button.
 *
 * Since we don't actually set column widths here, we need to tell the main
 * window that the defaults button was pushed.  It needs to reset all column
 * widths to defaults, and then take into account any checking and un-checking
 * that was done after "defaults" was pushed.
 */
void
PrefsGeneralPage::OnDefaults(void)
{
    LOGI("DEFAULTS!");

    CButton* pButton;

    fDefaultsPushed = true;

    ASSERT(IDC_COL_ACCESS == IDC_COL_PATHNAME + (kNumVisibleColumns-1));

    /* assumes that the controls are numbered sequentially */
    for (int i = 0; i < kNumVisibleColumns; i++) {
        pButton = (CButton*) GetDlgItem(IDC_COL_PATHNAME+i);
        ASSERT(pButton != NULL);
        pButton->SetCheck(1);   // 0=unchecked, 1=checked, 2=indeterminate
    }

    SetModified(TRUE);
}

/*
 * They clicked on "assocations".  Bring up the association edit dialog.
 * If they click "OK", make a copy of the changes and mark us as modified so
 * the Apply/Cancel buttons behave as expected.
 */
void
PrefsGeneralPage::OnAssociations(void)
{
    EditAssocDialog assocDlg;

    assocDlg.fOurAssociations = fOurAssociations;
    fOurAssociations = NULL;

    if (assocDlg.DoModal() == IDOK) {
        // steal the modified associations
        delete[] fOurAssociations;
        fOurAssociations = assocDlg.fOurAssociations;
        assocDlg.fOurAssociations = NULL;
        SetModified(TRUE);
    }
}

/*
 * Convert values.
 *
 * The various column checkboxes are independent.  We still do the xfer
 * for "pathname" even though it's disabled.
 */
void
PrefsGeneralPage::DoDataExchange(CDataExchange* pDX)
{
    fReady = true;

    ASSERT(NELEM(fColumn) == 9);
    DDX_Check(pDX, IDC_COL_PATHNAME, fColumn[0]);
    DDX_Check(pDX, IDC_COL_TYPE, fColumn[1]);
    DDX_Check(pDX, IDC_COL_AUXTYPE, fColumn[2]);
    DDX_Check(pDX, IDC_COL_MODDATE, fColumn[3]);
    DDX_Check(pDX, IDC_COL_FORMAT, fColumn[4]);
    DDX_Check(pDX, IDC_COL_SIZE, fColumn[5]);
    DDX_Check(pDX, IDC_COL_RATIO, fColumn[6]);
    DDX_Check(pDX, IDC_COL_PACKED, fColumn[7]);
    DDX_Check(pDX, IDC_COL_ACCESS, fColumn[8]);

    DDX_Check(pDX, IDC_PREF_SHRINKIT_COMPAT, fMimicShrinkIt);
    DDX_Check(pDX, IDC_PREF_SHK_BAD_MAC, fBadMacSHK);
    DDX_Check(pDX, IDC_PREF_REDUCE_SHK_ERROR_CHECKS, fReduceSHKErrorChecks);
    DDX_Check(pDX, IDC_PREF_COERCE_DOS, fCoerceDOSFilenames);
    DDX_Check(pDX, IDC_PREF_SPACES_TO_UNDER, fSpacesToUnder);
    DDX_Check(pDX, IDC_PREF_PASTE_JUNKPATHS, fPasteJunkPaths);
    DDX_Check(pDX, IDC_PREF_SUCCESS_BEEP, fBeepOnSuccess);
}

/*
 * Context help request (question mark button).
 */
LONG
PrefsGeneralPage::OnHelp(UINT wParam, LONG lParam)
{
    WinHelp((DWORD) ((HELPINFO*) lParam)->iCtrlId, HELP_CONTEXTPOPUP);
    return TRUE;    // yes, we handled it
}
/*
 * User pressed the PropertySheet "Help" button.
 */
LONG
PrefsGeneralPage::OnCommandHelp(UINT, LONG)
{
    WinHelp(HELP_TOPIC_PREFS_GENERAL, HELP_CONTEXT);
    return 0;       // doesn't matter
}


/*
 * ===========================================================================
 *      PrefsDiskImagePage
 * ===========================================================================
 */

BEGIN_MESSAGE_MAP(PrefsDiskImagePage, CPropertyPage)
    ON_BN_CLICKED(IDC_PDISK_CONFIRM_FORMAT, OnChange)
    ON_BN_CLICKED(IDC_PDISK_OPENVOL_RO, OnChange)
    ON_BN_CLICKED(IDC_PDISK_OPENVOL_PHYS0, OnChange)
    ON_BN_CLICKED(IDC_PDISK_PRODOS_ALLOWLOWER, OnChange)
    ON_BN_CLICKED(IDC_PDISK_PRODOS_USESPARSE, OnChange)
    ON_MESSAGE(WM_HELP, OnHelp)
    ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
END_MESSAGE_MAP()

/*
 * Set up our spin button.
 */
BOOL
PrefsDiskImagePage::OnInitDialog(void)
{
    //LOGI("OnInit!");
    return CPropertyPage::OnInitDialog();
}

/*
 * Enable the "apply" button.
 */
void
PrefsDiskImagePage::OnChange(void)
{
    LOGI("OnChange");
    SetModified(TRUE);
}
//void
//PrefsDiskImagePage::OnChangeRange(UINT nID)
//{
//  LOGI("OnChangeRange id=%d", nID);
//  SetModified(TRUE);
//}


/*
 * Convert values.
 */
void
PrefsDiskImagePage::DoDataExchange(CDataExchange* pDX)
{
    fReady = true;
    DDX_Check(pDX, IDC_PDISK_CONFIRM_FORMAT, fQueryImageFormat);
    DDX_Check(pDX, IDC_PDISK_OPENVOL_RO, fOpenVolumeRO);
    DDX_Check(pDX, IDC_PDISK_OPENVOL_PHYS0, fOpenVolumePhys0);
    DDX_Check(pDX, IDC_PDISK_PRODOS_ALLOWLOWER, fProDOSAllowLower);
    DDX_Check(pDX, IDC_PDISK_PRODOS_USESPARSE, fProDOSUseSparse);
}

/*
 * Context help request (question mark button).
 */
LONG
PrefsDiskImagePage::OnHelp(UINT wParam, LONG lParam)
{
    WinHelp((DWORD) ((HELPINFO*) lParam)->iCtrlId, HELP_CONTEXTPOPUP);
    return TRUE;    // yes, we handled it
}
/*
 * User pressed the PropertySheet "Help" button.
 */
LONG
PrefsDiskImagePage::OnCommandHelp(UINT, LONG)
{
    WinHelp(HELP_TOPIC_PREFS_DISK_IMAGE, HELP_CONTEXT);
    return 0;       // doesn't matter
}


/*
 * ===========================================================================
 *      PrefsCompressionPage
 * ===========================================================================
 */

BEGIN_MESSAGE_MAP(PrefsCompressionPage, CPropertyPage)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_DEFC_UNCOMPRESSED, IDC_DEFC_BZIP2, OnChangeRange)
    ON_MESSAGE(WM_HELP, OnHelp)
    ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
END_MESSAGE_MAP()


/*
 * Disable compression types not supported by the NufxLib DLL.
 */
BOOL
PrefsCompressionPage::OnInitDialog(void)
{
    if (!NufxArchive::IsCompressionSupported(kNuThreadFormatHuffmanSQ)) {
        DisableWnd(IDC_DEFC_SQUEEZE);
        if (fCompressType == kNuThreadFormatHuffmanSQ)
            fCompressType = kNuThreadFormatUncompressed;
    }
    if (!NufxArchive::IsCompressionSupported(kNuThreadFormatLZW1)) {
        DisableWnd(IDC_DEFC_LZW1);
        if (fCompressType == kNuThreadFormatLZW1)
            fCompressType = kNuThreadFormatUncompressed;
    }
    if (!NufxArchive::IsCompressionSupported(kNuThreadFormatLZW2)) {
        DisableWnd(IDC_DEFC_LZW2);
        if (fCompressType == kNuThreadFormatLZW2) {
            fCompressType = kNuThreadFormatUncompressed;
        }
    }
    if (!NufxArchive::IsCompressionSupported(kNuThreadFormatLZC12)) {
        DisableWnd(IDC_DEFC_LZC12);
        if (fCompressType == kNuThreadFormatLZC12)
            fCompressType = kNuThreadFormatUncompressed;
    }
    if (!NufxArchive::IsCompressionSupported(kNuThreadFormatLZC16)) {
        DisableWnd(IDC_DEFC_LZC16);
        if (fCompressType == kNuThreadFormatLZC16)
            fCompressType = kNuThreadFormatUncompressed;
    }
    if (!NufxArchive::IsCompressionSupported(kNuThreadFormatDeflate)) {
        DisableWnd(IDC_DEFC_DEFLATE);
        if (fCompressType == kNuThreadFormatDeflate)
            fCompressType = kNuThreadFormatUncompressed;
    }
    if (!NufxArchive::IsCompressionSupported(kNuThreadFormatBzip2)) {
        DisableWnd(IDC_DEFC_BZIP2);
        if (fCompressType == kNuThreadFormatBzip2)
            fCompressType = kNuThreadFormatUncompressed;
    }

    /* now invoke DoDataExchange with our modified fCompressType */
    return CPropertyPage::OnInitDialog();
}

/*
 * Disable a window in our dialog.
 */
void
PrefsCompressionPage::DisableWnd(int id)
{
    CWnd* pWnd;
    pWnd = GetDlgItem(id);
    if (pWnd == NULL) {
        ASSERT(false);
        return;
    }
    pWnd->EnableWindow(FALSE);
}

/*
 * Enable the "apply" button.
 */
void
PrefsCompressionPage::OnChangeRange(UINT nID)
{
    SetModified(TRUE);
}

/*
 * Convert values.
 *
 * Compression types match the NuThreadFormat enum in NufxLib.h, starting
 * with IDC_DEFC_UNCOMPRESSED.
 */
void
PrefsCompressionPage::DoDataExchange(CDataExchange* pDX)
{
    //LOGI("OnInit comp!");
    fReady = true;
    DDX_Radio(pDX, IDC_DEFC_UNCOMPRESSED, fCompressType);
}

/*
 * Context help request (question mark button).
 */
LONG
PrefsCompressionPage::OnHelp(UINT wParam, LONG lParam)
{
    WinHelp((DWORD) ((HELPINFO*) lParam)->iCtrlId, HELP_CONTEXTPOPUP);
    return TRUE;    // yes, we handled it
}
/*
 * User pressed the PropertySheet "Help" button.
 */
LONG
PrefsCompressionPage::OnCommandHelp(UINT, LONG)
{
    WinHelp(HELP_TOPIC_PREFS_COMPRESSION, HELP_CONTEXT);
    return 0;       // doesn't matter
}


/*
 * ===========================================================================
 *      PrefsFviewPage
 * ===========================================================================
 */

BEGIN_MESSAGE_MAP(PrefsFviewPage, CPropertyPage)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_PVIEW_NOWRAP_TEXT, IDC_PVIEW_HIRES_BW, OnChangeRange)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_PVIEW_HITEXT, IDC_PVIEW_TEXT8, OnChangeRange)
    ON_EN_CHANGE(IDC_PVIEW_SIZE_EDIT, OnChange)
    ON_CBN_SELCHANGE(IDC_PVIEW_DHR_CONV_COMBO, OnChange)
    ON_MESSAGE(WM_HELP, OnHelp)
    ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
END_MESSAGE_MAP()

/*
 * Set up our spin button.
 */
BOOL
PrefsFviewPage::OnInitDialog(void)
{
    //LOGI("OnInit!");
    CSpinButtonCtrl* pSpin;

    //LOGI("Configuring spin");

    pSpin = (CSpinButtonCtrl*) GetDlgItem(IDC_PVIEW_SIZE_SPIN);
    ASSERT(pSpin != NULL);

    UDACCEL uda;
    uda.nSec = 0;
    uda.nInc = 64;
    pSpin->SetRange(1, 32767);
    pSpin->SetAccel(1, &uda);
    LOGI("OnInit done!");

    return CPropertyPage::OnInitDialog();
}

/*
 * Enable the "apply" button.
 */
void
PrefsFviewPage::OnChange(void)
{
    LOGI("OnChange");
    SetModified(TRUE);
}
void
PrefsFviewPage::OnChangeRange(UINT nID)
{
    LOGI("OnChangeRange id=%d", nID);
    SetModified(TRUE);
}


/*
 * Convert values.
 */
void
PrefsFviewPage::DoDataExchange(CDataExchange* pDX)
{
    fReady = true;
    //DDX_Check(pDX, IDC_PVIEW_EOL_RAW, fEOLConvRaw);
    DDX_Check(pDX, IDC_PVIEW_NOWRAP_TEXT, fNoWrapText);
    DDX_Check(pDX, IDC_PVIEW_BOLD_HEXDUMP, fHighlightHexDump);
    DDX_Check(pDX, IDC_PVIEW_BOLD_BASIC, fHighlightBASIC);
    DDX_Check(pDX, IDC_PVIEW_DISASM_ONEBYTEBRKCOP, fConvDisasmOneByteBrkCop);
    DDX_Check(pDX, IDC_PVIEW_HIRES_BW, fConvHiResBlackWhite);
    DDX_CBIndex(pDX, IDC_PVIEW_DHR_CONV_COMBO, fConvDHRAlgorithm);

    DDX_Check(pDX, IDC_PVIEW_HITEXT, fConvTextEOL_HA);
    DDX_Check(pDX, IDC_PVIEW_CPMTEXT, fConvCPMText);
    DDX_Check(pDX, IDC_PVIEW_PASCALTEXT, fConvPascalText);
    DDX_Check(pDX, IDC_PVIEW_PASCALCODE, fConvPascalCode);
    DDX_Check(pDX, IDC_PVIEW_APPLESOFT, fConvApplesoft);
    DDX_Check(pDX, IDC_PVIEW_INTEGER, fConvInteger);
    DDX_Check(pDX, IDC_PVIEW_GWP, fConvGWP);
    DDX_Check(pDX, IDC_PVIEW_TEXT8, fConvText8);
    DDX_Check(pDX, IDC_PVIEW_AWP, fConvAWP);
    DDX_Check(pDX, IDC_PVIEW_ADB, fConvADB);
    DDX_Check(pDX, IDC_PVIEW_ASP, fConvASP);
    DDX_Check(pDX, IDC_PVIEW_SCASSEM, fConvSCAssem);
    DDX_Check(pDX, IDC_PVIEW_DISASM, fConvDisasm);

    DDX_Check(pDX, IDC_PVIEW_HIRES, fConvHiRes);
    DDX_Check(pDX, IDC_PVIEW_DHR, fConvDHR);
    DDX_Check(pDX, IDC_PVIEW_SHR, fConvSHR);
    DDX_Check(pDX, IDC_PVIEW_PRINTSHOP, fConvPrintShop);
    DDX_Check(pDX, IDC_PVIEW_MACPAINT, fConvMacPaint);
    DDX_Check(pDX, IDC_PVIEW_PRODOSFOLDER, fConvProDOSFolder);
    DDX_Check(pDX, IDC_PVIEW_RESOURCES, fConvResources);
    DDX_Check(pDX, IDC_PVIEW_RELAX_GFX, fRelaxGfxTypeCheck);

    DDX_Text(pDX, IDC_PVIEW_SIZE_EDIT, fMaxViewFileSizeKB);
    DDV_MinMaxUInt(pDX, fMaxViewFileSizeKB, 1, 32767);
}

/*
 * Context help request (question mark button).
 */
LONG
PrefsFviewPage::OnHelp(UINT wParam, LONG lParam)
{
    WinHelp((DWORD) ((HELPINFO*) lParam)->iCtrlId, HELP_CONTEXTPOPUP);
    return TRUE;    // yes, we handled it
}
/*
 * User pressed the PropertySheet "Help" button.
 */
LONG
PrefsFviewPage::OnCommandHelp(UINT, LONG)
{
    WinHelp(HELP_TOPIC_PREFS_FVIEW, HELP_CONTEXT);
    return 0;       // doesn't matter
}


/*
 * ===========================================================================
 *      PrefsFilesPage
 * ===========================================================================
 */

BEGIN_MESSAGE_MAP(PrefsFilesPage, CPropertyPage)
    ON_EN_CHANGE(IDC_PREF_TEMP_FOLDER, OnChange)
    ON_EN_CHANGE(IDC_PREF_EXTVIEWER_EXTS, OnChange)
    ON_BN_CLICKED(IDC_PREF_CHOOSE_TEMP_FOLDER, OnChooseFolder)
    ON_MESSAGE(WM_HELP, OnHelp)
    ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
END_MESSAGE_MAP()


/*
 * Set up the "choose folder" button.
 */
BOOL
PrefsFilesPage::OnInitDialog(void)
{
    fChooseFolderButton.ReplaceDlgCtrl(this, IDC_PREF_CHOOSE_TEMP_FOLDER);
    fChooseFolderButton.SetBitmapID(IDB_CHOOSE_FOLDER);

    return CPropertyPage::OnInitDialog();
}

/*
 * Enable the "apply" button.
 */
void
PrefsFilesPage::OnChange(void)
{
    SetModified(TRUE);
}

/*
 * Convert values.
 */
void
PrefsFilesPage::DoDataExchange(CDataExchange* pDX)
{
    fReady = true;
    DDX_Text(pDX, IDC_PREF_TEMP_FOLDER, fTempPath);
    DDX_Text(pDX, IDC_PREF_EXTVIEWER_EXTS, fExtViewerExts);

    /* validate the path field */
    if (pDX->m_bSaveAndValidate) {
        if (fTempPath.IsEmpty()) {
            CString appName;
            appName.LoadString(IDS_MB_APP_NAME);
            MessageBox(L"You must specify a path for temp files",
                appName, MB_OK);
            pDX->Fail();
        }

        // we *could* try to validate the path here...
    }
}

/*
 * They want to choose the folder from a menu hierarchy.  Show them a list.
 */
void
PrefsFilesPage::OnChooseFolder(void)
{
    ChooseDirDialog chooseDir(this);
    CWnd* pEditWnd;
    CString editPath;

    /* get the currently-showing text from the edit field */
    pEditWnd = GetDlgItem(IDC_PREF_TEMP_FOLDER);
    ASSERT(pEditWnd != NULL);
    pEditWnd->GetWindowText(editPath);

    chooseDir.SetPathName(editPath);
    if (chooseDir.DoModal() == IDOK) {
        const WCHAR* ccp = chooseDir.GetPathName();
        LOGI("New temp path chosen = '%ls'", ccp);

        pEditWnd->SetWindowText(ccp);

        // activate the "apply" button
        OnChange();
    }
}

/*
 * Context help request (question mark button).
 */
LONG
PrefsFilesPage::OnHelp(UINT wParam, LONG lParam)
{
    WinHelp((DWORD) ((HELPINFO*) lParam)->iCtrlId, HELP_CONTEXTPOPUP);
    return TRUE;    // yes, we handled it
}
/*
 * User pressed the PropertySheet "Help" button.
 */
LONG
PrefsFilesPage::OnCommandHelp(UINT, LONG)
{
    WinHelp(HELP_TOPIC_PREFS_FILES, HELP_CONTEXT);
    return 0;       // doesn't matter
}


/*
 * ===========================================================================
 *      PrefsSheet
 * ===========================================================================
 */

BEGIN_MESSAGE_MAP(PrefsSheet, CPropertySheet)
    ON_WM_NCCREATE()
    ON_BN_CLICKED(ID_APPLY_NOW, OnApplyNow)
    ON_COMMAND(ID_HELP, OnIDHelp)
    ON_MESSAGE(WM_HELP, OnHelp)
END_MESSAGE_MAP()

/*
 * Construct the preferences dialog from the individual pages.
 */
PrefsSheet::PrefsSheet(CWnd* pParentWnd) :
    CPropertySheet(L"Preferences", pParentWnd)
{
    AddPage(&fGeneralPage);
    AddPage(&fDiskImagePage);
    AddPage(&fFviewPage);
    AddPage(&fCompressionPage);
    AddPage(&fFilesPage);

    /* this happens automatically with appropriate ID_HELP handlers */
    //m_psh.dwFlags |= PSH_HASHELP;
}

/*
 * Enable the context help button.
 *
 * We don't seem to get a PreCreateWindow or OnInitDialog, but we can
 * intercept the WM_NCCREATE message and override the default behavior.
 */
BOOL
PrefsSheet::OnNcCreate(LPCREATESTRUCT cs)
{
    //LOGI("PrefsSheet OnNcCreate");
    BOOL val = CPropertySheet::OnNcCreate(cs);
    ModifyStyleEx(0, WS_EX_CONTEXTHELP);
    return val;
}

/*
 * Handle the "apply" button.  We only want to process updates for property
 * pages that have been constructed, and they only get constructed when
 * the user clicks on them.
 *
 * We also have to watch out for DDV tests that should prevent the "apply"
 * from succeeding, e.g. the file viewer size limit.
 */
void
PrefsSheet::OnApplyNow(void)
{
    BOOL result;

    if (fGeneralPage.fReady) {
        //LOGI("Apply to general?");
        result = fGeneralPage.UpdateData(TRUE);
        if (!result)
            return;
    }
    if (fDiskImagePage.fReady) {
        //LOGI("Apply to disk images?");
        result = fDiskImagePage.UpdateData(TRUE);
        if (!result)
            return;
    }
    if (fCompressionPage.fReady) {
        //LOGI("Apply to compression?");
        result = fCompressionPage.UpdateData(TRUE);
        if (!result)
            return;
    }
    if (fFviewPage.fReady) {
        //LOGI("Apply to fview?");
        result = fFviewPage.UpdateData(TRUE);
        if (!result)
            return;
    }

    if (fFilesPage.fReady) {
        //LOGI("Apply to fview?");
        result = fFilesPage.UpdateData(TRUE);
        if (!result)
            return;
    }

    /* reset all to "unmodified" state */
    LOGI("All 'applies' were successful");
    ((MainWindow*) AfxGetMainWnd())->ApplyNow(this);
    fGeneralPage.SetModified(FALSE);
    fGeneralPage.fDefaultsPushed = false;
    fDiskImagePage.SetModified(FALSE);
    fCompressionPage.SetModified(FALSE);
    fFviewPage.SetModified(FALSE);
    fFilesPage.SetModified(FALSE);
}

/*
 * Handle a press of the "Help" button by redirecting it back to ourselves
 * as a WM_COMMANDHELP message.  If we don't do this, the main window ends
 * up getting our WM_COMMAND(ID_HELP) message.
 *
 * We still need to define an ID_HELP WM_COMMAND handler in the main window,
 * or the CPropertySheet code refuses to believe that help is enabled for
 * the application as a whole.
 *
 * The PropertySheet object handles the WM_COMMANDHELP message and redirects
 * it to the active PropertyPage.  Each page must handle WM_COMMANDHELP by
 * opening an appropriate chapter in the help file.
 */
void
PrefsSheet::OnIDHelp(void)
{
    LOGI("PrefsSheet OnIDHelp");
    SendMessage(WM_COMMANDHELP);
}

/*
 * Context help request (question mark button) on something outside of the
 * property page, most likely the Apply or Cancel button.
 */
LONG
PrefsSheet::OnHelp(UINT wParam, LONG lParam)
{
    HELPINFO* lpHelpInfo = (HELPINFO*) lParam;

    LOGI("PrefsSheet OnHelp");
    DWORD context = lpHelpInfo->iCtrlId;
    WinHelp(context, HELP_CONTEXTPOPUP);

    return TRUE;    // yes, we handled it
}
