/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Edit file properties.
 */
#ifndef APP_EDITPROPSDIALOG_H
#define APP_EDITPROPSDIALOG_H

#include "GenericArchive.h"
#include "resource.h"

/*
 * Edit ProDOS file attributes, such as file type and auxtype.
 */
class EditPropsDialog : public CDialog {
public:
    typedef enum AllowedTypes {
        kAllowedUnknown = 0,
        kAllowedProDOS,     // 8-bit type, 16-bit aux
        kAllowedHFS,        // 32-bit type, 32-bit aux
        kAllowedNone,       // CP/M
        kAllowedPascal,     // UCSD Pascal
        kAllowedDOS,        // DOS 3.2/3.3
    } AllowedTypes;

    EditPropsDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_PROPS_EDIT, pParentWnd)
    {
        memset(&fProps, 0, sizeof(fProps));
        fReadOnly = false;
        fAllowedTypes = kAllowedProDOS;
        fSimpleAccess = false;
        fNoChangeAccess = false;
        fAllowInvis = false;
        //fHFSMode = false;
        fHFSComboIdx = -1;
    }
    ~EditPropsDialog(void) {}

    /* these get handed to GenericArchive */
    FileProps   fProps;

    /* initialize fProps and other fields from pEntry */
    void InitProps(GenericEntry* pEntry);

    /* set this to disable editing of all fields */
    bool    fReadOnly;

private:
    virtual BOOL OnInitDialog(void) override;
    virtual void DoDataExchange(CDataExchange* pDX) override;

    /*
     * This is called when the file type selection changes or something is
     * typed in the aux type box.
     *
     * We use this notification to configure the type description field.
     *
     * Typing in the ProDOS aux type box causes us to nuke the HFS values.
     * If we were in "HFS mode" we reset the file type to zero.
     */
    afx_msg void OnTypeChange(void);

    /*
     * Called when something is typed in one of the HFS type boxes.
     */
    afx_msg void OnHFSTypeChange(void);

    afx_msg void OnHelp(void) {
        MyApp::HandleHelp(this, HELP_TOPIC_EDIT_PROPS);
    }
    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo) {
        return MyApp::HandleHelpInfo(lpHelpInfo);
    }

    /*
     * For "simple" access formats, i.e. DOS 3.2/3.3, the "write" button acts
     * as a "locked" flag.  We want the other rename/delete flags to track this
     * one.
     */
    void UpdateSimpleAccess(void);

    /*
     * Called initially and when switching modes.
     */
    void UpdateHFSMode(void);

    /*
     * Get the aux type.
     *
     * Returns -1 if something was wrong with the string (e.g. empty or has
     * invalid chars).
     */
    long GetAuxType(void);

    /* what sort of type changes do we allow? */
    AllowedTypes    fAllowedTypes;
    /* set this to disable access to fields other than 'W' */
    bool        fSimpleAccess;
    /* set this to disable file access fields */
    bool        fNoChangeAccess;
    /* this enabled the 'I' flag, independent of other settings */
    bool        fAllowInvis;

    /* are we in "ProDOS mode" or "HFS mode"? */
    //bool      fHFSMode;
    /* fake file type entry that says "(HFS)" */
    int         fHFSComboIdx;

    /* these are displayed locally */
    CString     fPathName;
    CString     fAuxType;       // DDX doesn't do hex conversion

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_EDITPROPSDIALOG_H*/
