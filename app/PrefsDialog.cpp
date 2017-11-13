/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#include "stdafx.h"
#include "PrefsDialog.h"
#include "ChooseDirDialog.h"
#ifdef CAN_UPDATE_FILE_ASSOC
#include "EditAssocDialog.h"
#endif
#include "Main.h"
#include "NufxArchive.h"
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
#ifdef CAN_UPDATE_FILE_ASSOC
    ON_BN_CLICKED(IDC_PREF_ASSOCIATIONS, OnAssociations)
#endif
    ON_MESSAGE(WM_HELP, OnHelpInfo)
    ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
END_MESSAGE_MAP()


void PrefsGeneralPage::OnChange(void)
{
    /*
     * They clicked on a checkbox, just mark the page as dirty so the "apply"
     * button will be enabled.
     */
    SetModified(TRUE);
}

void PrefsGeneralPage::OnChangeRange(UINT nID)
{
    SetModified(TRUE);
}

void PrefsGeneralPage::OnDefaults(void)
{
    /*
     * Since we don't actually set column widths here, we need to tell the main
     * window that the defaults button was pushed.  It needs to reset all column
     * widths to defaults, and then take into account any checking and un-checking
     * that was done after "defaults" was pushed.
     */
    LOGD("OnDefaults");

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

#ifdef CAN_UPDATE_FILE_ASSOC
void PrefsGeneralPage::OnAssociations(void)
{
    EditAssocDialog assocDlg;

    assocDlg.fOurAssociations = fOurAssociations;
    fOurAssociations = NULL;

    if (assocDlg.DoModal() == IDOK) {
        /*
         * Make a copy of the changes and mark us as modified so
         * the Apply/Cancel buttons behave as expected.  (We don't make
         * a copy so much as steal the data from the dialog object.)
         */
        delete[] fOurAssociations;
        fOurAssociations = assocDlg.fOurAssociations;
        assocDlg.fOurAssociations = NULL;
        SetModified(TRUE);
    }
}
#endif

void PrefsGeneralPage::DoDataExchange(CDataExchange* pDX)
{
    /*
     * The various column checkboxes are independent.  We still do the xfer
     * for "pathname" even though it's disabled.
     */

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
    ON_MESSAGE(WM_HELP, OnHelpInfo)
    ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
END_MESSAGE_MAP()

BOOL PrefsDiskImagePage::OnInitDialog(void)
{
    //LOGI("OnInit!");
    return CPropertyPage::OnInitDialog();
}

void PrefsDiskImagePage::OnChange(void)
{
    LOGD("OnChange");
    SetModified(TRUE);  // enable the "apply" button
}

//void PrefsDiskImagePage::OnChangeRange(UINT nID)
//{
//  LOGD("OnChangeRange id=%d", nID);
//  SetModified(TRUE);
//}


void PrefsDiskImagePage::DoDataExchange(CDataExchange* pDX)
{
    fReady = true;
    DDX_Check(pDX, IDC_PDISK_CONFIRM_FORMAT, fQueryImageFormat);
    DDX_Check(pDX, IDC_PDISK_OPENVOL_RO, fOpenVolumeRO);
    DDX_Check(pDX, IDC_PDISK_OPENVOL_PHYS0, fOpenVolumePhys0);
    DDX_Check(pDX, IDC_PDISK_PRODOS_ALLOWLOWER, fProDOSAllowLower);
    DDX_Check(pDX, IDC_PDISK_PRODOS_USESPARSE, fProDOSUseSparse);
}


/*
 * ===========================================================================
 *      PrefsCompressionPage
 * ===========================================================================
 */

BEGIN_MESSAGE_MAP(PrefsCompressionPage, CPropertyPage)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_DEFC_UNCOMPRESSED, IDC_DEFC_BZIP2, OnChangeRange)
    ON_MESSAGE(WM_HELP, OnHelpInfo)
    ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
END_MESSAGE_MAP()


BOOL PrefsCompressionPage::OnInitDialog(void)
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

void PrefsCompressionPage::DisableWnd(int id)
{
    CWnd* pWnd;
    pWnd = GetDlgItem(id);
    if (pWnd == NULL) {
        ASSERT(false);
        return;
    }
    pWnd->EnableWindow(FALSE);
}

void PrefsCompressionPage::OnChangeRange(UINT nID)
{
    SetModified(TRUE);      // enable the "apply" button
}

void PrefsCompressionPage::DoDataExchange(CDataExchange* pDX)
{
    /*
     * Compression types match the NuThreadFormat enum in NufxLib.h, starting
     * with IDC_DEFC_UNCOMPRESSED.
     */

    LOGV("OnInit comp!");
    fReady = true;
    DDX_Radio(pDX, IDC_DEFC_UNCOMPRESSED, fCompressType);
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
    ON_MESSAGE(WM_HELP, OnHelpInfo)
    ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
END_MESSAGE_MAP()

BOOL PrefsFviewPage::OnInitDialog(void)
{
    CSpinButtonCtrl* pSpin;

    LOGV("Configuring spin");

    pSpin = (CSpinButtonCtrl*) GetDlgItem(IDC_PVIEW_SIZE_SPIN);
    ASSERT(pSpin != NULL);

    UDACCEL uda;
    uda.nSec = 0;
    uda.nInc = 64;
    pSpin->SetRange(1, 32767);
    pSpin->SetAccel(1, &uda);
    LOGD("OnInit done!");

    return CPropertyPage::OnInitDialog();
}

void PrefsFviewPage::OnChange(void)
{
    LOGD("OnChange");
    SetModified(TRUE);      // enable the "apply" button
}

void PrefsFviewPage::OnChangeRange(UINT nID)
{
    LOGD("OnChangeRange id=%d", nID);
    SetModified(TRUE);
}

void PrefsFviewPage::DoDataExchange(CDataExchange* pDX)
{
    fReady = true;
    //DDX_Check(pDX, IDC_PVIEW_EOL_RAW, fEOLConvRaw);
    DDX_Check(pDX, IDC_PVIEW_NOWRAP_TEXT, fNoWrapText);
    DDX_Check(pDX, IDC_PVIEW_BOLD_HEXDUMP, fHighlightHexDump);
    DDX_Check(pDX, IDC_PVIEW_BOLD_BASIC, fHighlightBASIC);
    DDX_Check(pDX, IDC_PVIEW_DISASM_ONEBYTEBRKCOP, fConvDisasmOneByteBrkCop);
    DDX_Check(pDX, IDC_PVIEW_MOUSETEXT_TO_ASCII, fConvMouseTextToASCII);
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
 * ===========================================================================
 *      PrefsFilesPage
 * ===========================================================================
 */

BEGIN_MESSAGE_MAP(PrefsFilesPage, CPropertyPage)
    ON_EN_CHANGE(IDC_PREF_TEMP_FOLDER, OnChange)
    ON_EN_CHANGE(IDC_PREF_EXTVIEWER_EXTS, OnChange)
    ON_BN_CLICKED(IDC_PREF_CHOOSE_TEMP_FOLDER, OnChooseFolder)
    ON_MESSAGE(WM_HELP, OnHelpInfo)
    ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
END_MESSAGE_MAP()


BOOL PrefsFilesPage::OnInitDialog(void)
{
    fChooseFolderButton.ReplaceDlgCtrl(this, IDC_PREF_CHOOSE_TEMP_FOLDER);
    fChooseFolderButton.SetBitmapID(IDB_CHOOSE_FOLDER);

    return CPropertyPage::OnInitDialog();
}

void PrefsFilesPage::OnChange(void)
{
    SetModified(TRUE);      // enable the "apply" button
}

void PrefsFilesPage::DoDataExchange(CDataExchange* pDX)
{
    fReady = true;
    DDX_Text(pDX, IDC_PREF_TEMP_FOLDER, fTempPath);
    DDX_Text(pDX, IDC_PREF_EXTVIEWER_EXTS, fExtViewerExts);

    /* validate the path field */
    if (pDX->m_bSaveAndValidate) {
        if (fTempPath.IsEmpty()) {
            CString appName;
            CheckedLoadString(&appName, IDS_MB_APP_NAME);
            MessageBox(L"You must specify a path for temp files",
                appName, MB_OK);
            pDX->Fail();
        }

        // we *could* try to validate the path here...
    }
}

void PrefsFilesPage::OnChooseFolder(void)
{
    /*
     * They want to choose the folder from a menu hierarchy.  Show them a list.
     */

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
        LOGD("New temp path chosen = '%ls'", ccp);

        pEditWnd->SetWindowText(ccp);

        // activate the "apply" button
        OnChange();
    }
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
    ON_MESSAGE(WM_HELP, OnHelpInfo)
END_MESSAGE_MAP()

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

BOOL PrefsSheet::OnNcCreate(LPCREATESTRUCT cs)
{
    LOGV("PrefsSheet OnNcCreate");
    BOOL val = CPropertySheet::OnNcCreate(cs);
    ModifyStyleEx(0, WS_EX_CONTEXTHELP);
    return val;
}

void PrefsSheet::OnApplyNow(void)
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
    LOGD("All 'applies' were successful");
    ((MainWindow*) AfxGetMainWnd())->ApplyNow(this);
    fGeneralPage.SetModified(FALSE);
    fGeneralPage.fDefaultsPushed = false;
    fDiskImagePage.SetModified(FALSE);
    fCompressionPage.SetModified(FALSE);
    fFviewPage.SetModified(FALSE);
    fFilesPage.SetModified(FALSE);
}

void PrefsSheet::OnIDHelp(void)
{
    LOGD("PrefsSheet OnIDHelp");
    SendMessage(WM_COMMANDHELP);
}
