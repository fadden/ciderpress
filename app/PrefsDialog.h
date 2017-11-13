/*
 * CiderPress
 * Copyright (C) 2007, 2008 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Classes to support the Preferences property pages.
 */
#ifndef APP_PREFSDIALOG_H
#define APP_PREFSDIALOG_H

#include "Preferences.h"
#include "../util/UtilLib.h"
#include "resource.h"
#include "HelpTopics.h"

/*
 * The "general" page, which controls how we display information to the user.
 */
class PrefsGeneralPage : public CPropertyPage
{
public:
    PrefsGeneralPage(void) :
      CPropertyPage(IDD_PREF_GENERAL),
      fReady(false),
      fMimicShrinkIt(FALSE),
      fBadMacSHK(FALSE),
      fReduceSHKErrorChecks(FALSE),
      fCoerceDOSFilenames(FALSE),
      fSpacesToUnder(FALSE),
      fDefaultsPushed(FALSE),
      fOurAssociations(NULL)
        {}
    virtual ~PrefsGeneralPage(void) {
        delete[] fOurAssociations;
    }

    bool    fReady;

    // fields on this page
    BOOL    fColumn[kNumVisibleColumns];
    BOOL    fMimicShrinkIt;
    BOOL    fBadMacSHK;
    BOOL    fReduceSHKErrorChecks;
    BOOL    fCoerceDOSFilenames;
    BOOL    fSpacesToUnder;
    BOOL    fPasteJunkPaths;
    BOOL    fBeepOnSuccess;
    BOOL    fDefaultsPushed;

    // initialized if we opened the file associations edit page
    bool*   fOurAssociations;

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;

    afx_msg void OnChange(void);
    afx_msg void OnChangeRange(UINT);
    afx_msg void OnDefaults(void);
#ifdef CAN_UPDATE_FILE_ASSOC
    afx_msg void OnAssociations(void);
#endif
    afx_msg LONG OnHelpInfo(UINT wParam, LONG lParam) {
        return MyApp::HandleHelpInfo((HELPINFO*) lParam);
    }
    afx_msg LONG OnCommandHelp(UINT wParam, LONG lParam) {
        MyApp::HandleHelp(this, HELP_TOPIC_PREFS_GENERAL);
        return TRUE;
    }

    DECLARE_MESSAGE_MAP()
};

/*
 * The "disk image" page, for selecting disk image preferences.
 */
class PrefsDiskImagePage : public CPropertyPage
{
public:
    PrefsDiskImagePage(void) :
      CPropertyPage(IDD_PREF_DISKIMAGE),
      fReady(false),
      fQueryImageFormat(FALSE),
      fOpenVolumeRO(FALSE),
      fProDOSAllowLower(FALSE),
      fProDOSUseSparse(FALSE)
      {}

    bool    fReady;

    BOOL    fQueryImageFormat;
    BOOL    fOpenVolumeRO;
    BOOL    fOpenVolumePhys0;
    BOOL    fProDOSAllowLower;
    BOOL    fProDOSUseSparse;

protected:
    virtual BOOL OnInitDialog(void) override;
    virtual void DoDataExchange(CDataExchange* pDX) override;

    afx_msg void OnChange(void);
    //afx_msg void OnChangeRange(UINT);
    afx_msg LONG OnHelpInfo(UINT wParam, LONG lParam) {
        return MyApp::HandleHelpInfo((HELPINFO*) lParam);
    }
    afx_msg LONG OnCommandHelp(UINT wParam, LONG lParam) {
        MyApp::HandleHelp(this, HELP_TOPIC_PREFS_DISK_IMAGE);
        return TRUE;
    }

    DECLARE_MESSAGE_MAP()
};

/*
 * The "compression" page, which lets the user choose a default compression
 * method.
 */
class PrefsCompressionPage : public CPropertyPage
{
public:
    PrefsCompressionPage(void) :
      CPropertyPage(IDD_PREF_COMPRESSION), fReady(false)
      {}

    bool    fReady;

    int     fCompressType;      // radio button index

protected:
    /*
     * Disable compression types not supported by the NufxLib DLL.
     */
    virtual BOOL OnInitDialog(void) override;

    virtual void DoDataExchange(CDataExchange* pDX) override;

    afx_msg void OnChangeRange(UINT);
    afx_msg LONG OnHelpInfo(UINT wParam, LONG lParam) {
        return MyApp::HandleHelpInfo((HELPINFO*) lParam);
    }
    afx_msg LONG OnCommandHelp(UINT wParam, LONG lParam) {
        MyApp::HandleHelp(this, HELP_TOPIC_PREFS_COMPRESSION);
        return TRUE;
    }

private:
    /*
     * Disable a window in our dialog.
     */
    void DisableWnd(int id);

    DECLARE_MESSAGE_MAP()
};

/*
 * The "fview" page, for selecting preferences for the internal file viewer.
 */
class PrefsFviewPage : public CPropertyPage
{
public:
    PrefsFviewPage(void) :
      CPropertyPage(IDD_PREF_FVIEW), fReady(false)
    {}
    bool    fReady;

    BOOL    fEOLConvRaw;
    BOOL    fNoWrapText;
    BOOL    fHighlightHexDump;
    BOOL    fHighlightBASIC;
    BOOL    fConvDisasmOneByteBrkCop;
    BOOL    fConvMouseTextToASCII;
    BOOL    fConvHiResBlackWhite;
    int     fConvDHRAlgorithm;      // drop list

    BOOL    fConvTextEOL_HA;
    BOOL    fConvCPMText;
    BOOL    fConvPascalText;
    BOOL    fConvPascalCode;
    BOOL    fConvApplesoft;
    BOOL    fConvInteger;
    BOOL    fConvBusiness;
    BOOL    fConvGWP;
    BOOL    fConvText8;
    BOOL    fConvAWP;
    BOOL    fConvADB;
    BOOL    fConvASP;
    BOOL    fConvSCAssem;
    BOOL    fConvDisasm;

    BOOL    fConvHiRes;
    BOOL    fConvDHR;
    BOOL    fConvSHR;
    BOOL    fConvPrintShop;
    BOOL    fConvMacPaint;
    BOOL    fConvProDOSFolder;
    BOOL    fConvResources;
    BOOL    fRelaxGfxTypeCheck;

    UINT    fMaxViewFileSizeKB;

protected:
    virtual BOOL OnInitDialog(void) override;
    virtual void DoDataExchange(CDataExchange* pDX) override;

    afx_msg void OnChange(void);
    afx_msg void OnChangeRange(UINT);
    afx_msg LONG OnHelpInfo(UINT wParam, LONG lParam) {
        return MyApp::HandleHelpInfo((HELPINFO*) lParam);
    }
    afx_msg LONG OnCommandHelp(UINT wParam, LONG lParam) {
        MyApp::HandleHelp(this, HELP_TOPIC_PREFS_FVIEW);
        return TRUE;
    }

    DECLARE_MESSAGE_MAP()
};

/*
 * The "compression" page, which lets the user choose a default compression
 * method for NuFX archives.
 */
class PrefsFilesPage : public CPropertyPage
{
public:
    PrefsFilesPage(void) :
      CPropertyPage(IDD_PREF_FILES), fReady(false)
      {}

    bool    fReady;

    CString fTempPath;
    CString fExtViewerExts;

protected:
    virtual BOOL OnInitDialog(void) override;
    virtual void DoDataExchange(CDataExchange* pDX) override;

    afx_msg void OnChange(void);
    afx_msg void OnChooseFolder(void);
    afx_msg LONG OnHelpInfo(UINT wParam, LONG lParam) {
        return MyApp::HandleHelpInfo((HELPINFO*) lParam);
    }
    afx_msg LONG OnCommandHelp(UINT wParam, LONG lParam) {
        MyApp::HandleHelp(this, HELP_TOPIC_PREFS_FILES);
        return TRUE;
    }

    MyBitmapButton  fChooseFolderButton;

    DECLARE_MESSAGE_MAP()
};


/*
 * Property sheet that wraps around the preferences pages.
 */
class PrefsSheet : public CPropertySheet
{
public:
    /*
     * Construct the preferences dialog from the individual pages.
     */
    PrefsSheet(CWnd* pParentWnd = NULL);

    PrefsGeneralPage        fGeneralPage;
    PrefsDiskImagePage      fDiskImagePage;
    PrefsCompressionPage    fCompressionPage;
    PrefsFviewPage          fFviewPage;
    PrefsFilesPage          fFilesPage;

protected:
    /*
     * Enables the context help button.
     *
     * We don't seem to get a PreCreateWindow or OnInitDialog, but we can
     * intercept the WM_NCCREATE message and override the default behavior.
     */
    afx_msg BOOL OnNcCreate(LPCREATESTRUCT cs);

    /*
     * Handle the "apply" button.  We only want to process updates for property
     * pages that have been constructed, and they only get constructed when
     * the user clicks on them.
     *
     * We also have to watch out for DDV tests that should prevent the "apply"
     * from succeeding, e.g. the file viewer size limit.
     */
    afx_msg void OnApplyNow();

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
    afx_msg void OnIDHelp(void);

    /*
     * Context help request (question mark button) on something outside of the
     * property page, most likely the Apply or Cancel button.
     */
    afx_msg LONG OnHelpInfo(UINT wParam, LONG lParam) {
        return MyApp::HandleHelpInfo((HELPINFO*) lParam);
    }

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_PREFSDIALOG_H*/
