/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * File selection dialog, a sub-class of "Open" that allows multiple selection
 * of both files and directories.
 *
 * This is something of a nightmare to work through.  The standard Windows
 * dialog will return multiple selected files, but omits the directories,
 * leaving the developer to find alternative means of acquiring the complete
 * list of files.  The most popular approach is to dig into the CListView
 * object (lst2) and peruse the set of selected files from the control itself.
 *
 * Complicating this is the existence of three very different dialog
 * implementations, known as "old style", "explorer" and "vista".  Since
 * we are currently targeting WinXP as a minimum OS level, and I would
 * prefer not to have multiple implementations, this code targets the
 * explorer-style dialogs.
 *
 * The API follows the standard file dialog multi-select pattern, which
 * returned a directory name followed by a series of files in that directory.
 * We simplify things a bit by returning the pathname separately and the
 * filenames in a string array.
 *
 * The current implementation owes a debt to Hojjat Bohlooli's
 * SelectDialog sample (http://www.codeproject.com/Articles/28015/).
 *
 * Other relevant links:
 *  http://www.codeproject.com/dialog/select_all_button.asp
 *  http://www.codeproject.com/dialog/select_all_button.asp
 *  http://www.codeproject.com/dialog/customize_dialog.asp
 *  http://stackoverflow.com/questions/31059/
 *
 * I wish I could say all this nonsense was fixed in the "vista" dialogs,
 * but it isn't (e.g. http://stackoverflow.com/questions/8269696/ ).
 */
#ifndef UTIL_SELECTFILESDIALOG_H
#define UTIL_SELECTFILESDIALOG_H

/*
 * File selection, based on an "open" file dialog.
 *
 * DoModal will return with IDCANCEL to indicate that the standard fields
 * (like lpstrFile) are not actually filled in.  Use the provided fields
 * to get exit status and filename info.
 *
 * Defer any dialog initialization to MyDataExchange.
 */
class SelectFilesDialog : public CFileDialog {
public:
    enum { kFileNameBufSize = 32768 };
    SelectFilesDialog(const WCHAR* rctmpl, CWnd* pParentWnd = NULL) :
        CFileDialog(true, NULL, NULL, OFN_HIDEREADONLY, NULL, pParentWnd,
            0, FALSE /*disable Vista style*/)
    {
        m_ofn.Flags |= OFN_ENABLETEMPLATE | OFN_ALLOWMULTISELECT |
            OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING;
        m_ofn.lpTemplateName = rctmpl;
        m_ofn.hInstance = AfxGetInstanceHandle();
        m_ofn.lpstrFile = new WCHAR[kFileNameBufSize];
        m_ofn.lpstrFile[0] = m_ofn.lpstrFile[1] = '\0';
        m_ofn.nMaxFile = kFileNameBufSize;
        m_ofn.nFileOffset = -1;
        m_ofn.Flags |= OFN_ENABLEHOOK;
        m_ofn.lpfnHook = OFNHookProc;
        m_ofn.lCustData = (LPARAM) this;

        fExitStatus = IDABORT;
        fFileNames = new WCHAR[2];
        fFileNames[0] = fFileNames[1] = '\0';
        fFileNameOffset = 0;

        fAcceptButtonID = IDOK;

        fReady = false;     // used by WM_SIZE handler
    }
    virtual ~SelectFilesDialog(void) {
        delete[] m_ofn.lpstrFile;
        delete[] fFileNames;
    }

    int GetExitStatus(void) const { return fExitStatus; }
    const WCHAR* GetFileNames(void) const { return fFileNames; }
    int GetFileNameOffset(void) const { return fFileNameOffset; }

    // set the window title; must be called before DoModal
    void SetWindowTitle(const WCHAR* title) {
        m_ofn.lpstrTitle = title;
    }

    // stuff values into our filename holder
    void SetFileNames(const WCHAR* fileNames, size_t len, int fileNameOffset) {
        ASSERT(len > wcslen(fileNames));
        ASSERT(fileNames[len] == '\0');
        ASSERT(fileNames[len-1] == '\0');
        LOGD("SetFileNames '%ls' %d %d", fileNames, len, fileNameOffset);
        delete[] fFileNames;
        fFileNames = new WCHAR[len];
        memcpy(fFileNames, fileNames, len * sizeof(WCHAR));
        fFileNameOffset = fileNameOffset;
    }

protected:
    // would be overrides
    virtual void MyOnInitDone(void);
    virtual void MyOnFileNameChange(void);
    // like DoDataExchange, but ret val replaces pDX->Fail()
    virtual bool MyDataExchange(bool saveAndValidate) { return true; }

    // would be message handlers
    virtual UINT HandleNotify(HWND hDlg, LPOFNOTIFY pofn);
    virtual UINT HandleCommand(HWND hDlg, WPARAM wParam, LPARAM lParam);
    virtual UINT HandleSize(HWND hDlg, UINT nType, int cx, int cy);
    virtual UINT HandleHelp(HWND hDlg, LPHELPINFO lpHelp);
    virtual UINT MyOnCommand(WPARAM wParam, LPARAM lParam) { return 0; }

    virtual void MyOnAccept(void);
    virtual void MyOnCancel(void);

    // utility functions
    virtual CWnd* GetListCtrl(void);
    virtual void ShiftControls(int deltaX, int deltaY);
    virtual void DestroyItem(CWnd* pDlg, int id) {
        CWnd* pWnd = pDlg->GetDlgItem(id);
        if (pWnd == NULL) {
            LOGW("Could not find item %p %d", pDlg, id);
            return;
        }
        pWnd->DestroyWindow();
    }
    virtual bool PrepEndDialog(void);

    // make this a little easier
    int MessageBox(LPCTSTR lpszText, LPCTSTR lpszCaption = NULL,
        UINT nType = MB_OK)
    {
        return GetParent()->MessageBox(lpszText, lpszCaption, nType);
    }

    // we're in a library, so the app resources have to tell us this
    int     fAcceptButtonID;

private:
    static UINT CALLBACK OFNHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam,
        LPARAM lParam);
    void ClearFileName(void);

    bool    fReady;
    int     fExitStatus;
    int     fFileNameOffset;
    WCHAR*  fFileNames;
    CRect   fLastWinSize;

    //DECLARE_MESSAGE_MAP()
};


/*
 * File selection, based on an "open" common file dialog.
 */
class SelectFilesDialog2 : public CFileDialog {
public:
    SelectFilesDialog2(const WCHAR* rctmpl, CWnd* pParentWnd = NULL) :
        CFileDialog(true, NULL, NULL, OFN_HIDEREADONLY, NULL, pParentWnd,
            0, FALSE /*disable Vista style*/)
    {
        // Set flags.  We specify ALLOWMULTISELECT but no filename buffer;
        // we want the multi-select behavior but we don't want to return
        // the filenames in the filename buf.
        m_ofn.Flags |= OFN_ENABLETEMPLATE | OFN_ALLOWMULTISELECT |
            OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING;

        // Configure use of a template.  The dialog template must have
        // WS_CHILD and WS_CLIPSIBLINGS set, and should have DS_3DLOOK and
        // DS_CONTROL set as well.
        m_ofn.lpTemplateName = rctmpl;
        m_ofn.hInstance = AfxGetInstanceHandle();
    }

    virtual ~SelectFilesDialog2(void) {}

    /*
     * Gets the directory name where the files live.  This is a full path.
     */
    const CString& GetDirectory(void) const { return fCurrentDirectory;  }

    /*
     * Gets the file name array.  This is only valid if the dialog exited
     * successfully (DoModal returned IDOK).
     */
    const CStringArray& GetFileNames(void) const { return fFileNameArray; }

    /*
     * Sets the window title; must be called before DoModal.
     */
    void SetWindowTitle(const WCHAR* title) {
        fCustomTitle = title;
        m_ofn.lpstrTitle = (LPCWSTR) fCustomTitle;
    }

protected:
    /*
     * Finish configuring the file dialog.
     */
    virtual void OnInitDone() override;

    /*
     * Track changes to the current directory.
     *
     * Updates fCurrentDirectory with the path currently being used by the
     * dialog.  For items with no path (e.g. Computer or Libraries), this
     * will set an empty string.
     */
    virtual void OnFolderChange() override;

private:
    /*
     * Custom filename validation; in our case, it's a double-click trap.
     *
     * Returns 0 if the name looks good, 1 otherwise.  If we return 1, the
     * dialog will not close.
     */
    virtual BOOL OnFileNameOK() override;

    /*
     * Our WindowProc callback function.  Watches for the OK button.
     */
    static LRESULT CALLBACK MyWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
        LPARAM lParam);

    // filename parse result
    enum FPResult { kFPDone, kFPPassThru, kFPNoFiles, kFPError };

    /*
     * The "OK" (actually "Open" or "Accept") button was clicked.  Extract
     * the filenames from the list control.
     *
     * Possible return values:
     *  OK_DONE
     *    We have successfully obtained the list of files and folders.
     *  OK_PASSTHRU
     *    Let the parent dialog process the event.  This is done when the
     *    edit box contains a directory name -- we want the dialog to
     *    change to that directory.
     *  OK_NOFILES
     *    No files were selected.  Keep the dialog open.
     * OK_ERROR
     *    Something went wrong.
     */
    FPResult OKButtonClicked(CFileDialog* pDialog);

    /*
     * Parses the file name string returned by the dialog.  Adds them to
     * fPathNameArray.  Returns the number of names found, or -1 if the
     * string appears to be invalid.
     */
    int ParseFileNames(const CString& str);

    /*
     * Previous WindowProc.  Most messages will be forwarded to this.
     */
    WNDPROC         fPrevWndProc;

    CString         fCustomTitle;

    // Directory the dialog is currently accessing.  Prepend this to the
    // entries in fFileNameArray to get the full path.
    CString         fCurrentDirectory;

    // File names of selected files.
    CStringArray    fFileNameArray;
};

#endif /*UTIL_SELECTFILESDIALOG_H*/
