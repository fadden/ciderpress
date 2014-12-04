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
 * File selection, based on an "open" common file dialog.
 */
class SelectFilesDialog : public CFileDialog {
public:
    SelectFilesDialog(const WCHAR* rctmpl, bool showHelp, CWnd* pParentWnd = NULL) :
        CFileDialog(true, NULL, NULL, OFN_HIDEREADONLY, NULL, pParentWnd,
            0, FALSE /*disable Vista style*/)
    {
        // Set flags.  We specify ALLOWMULTISELECT but no filename buffer;
        // we want the multi-select behavior but we don't want to return
        // the filenames in the filename buf.
        m_ofn.Flags |= OFN_ENABLETEMPLATE | OFN_ALLOWMULTISELECT |
            OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING |
            (showHelp ? OFN_SHOWHELP : 0);

        // Configure use of a template.  The dialog template must have
        // WS_CHILD and WS_CLIPSIBLINGS set, and should have DS_3DLOOK and
        // DS_CONTROL set as well.
        m_ofn.lpTemplateName = rctmpl;
        m_ofn.hInstance = AfxGetInstanceHandle();
    }

    virtual ~SelectFilesDialog(void) {}

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

    /*
     * Stuffs a single file/directory into the return value fields.  This is
     * (mis-)used by code that treats AddFilesDialog as a way to pass data
     * around.
     *
     * TODO: don't do this -- change the callers to pass data around differently
     */
    void StuffSingleFilename(const CString& directory, const CString& filename) {
        fCurrentDirectory = directory;
        fFileNameArray.RemoveAll();
        fFileNameArray.Add(filename);
    }

protected:
    // This is much like DoDataExchange, but ret val replaces pDX->Fail().
    // This will be called with saveAndValidate==true during OnInitDialog,
    // and with false when we've extracted the filenames and are about to
    // close the dialog.
    //
    // Return true on success, false if something failed and we want to keep
    // the dialog open.
    virtual bool MyDataExchange(bool saveAndValidate) { return true; }

    // Handles a click on the "help" button.
    virtual void HandleHelp() {}

private:
    DECLARE_COPY_AND_OPEQ(SelectFilesDialog)

    /*
     * Finishes configuring the file dialog.
     */
    virtual void OnInitDone() override;

    /*
     * Tracks changes to the current directory.
     *
     * Updates fCurrentDirectory with the path currently being used by the
     * dialog.  For items with no path (e.g. Computer or Libraries), this
     * will set an empty string.
     */
    virtual void OnFolderChange() override;

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
    // entries in fFileNameArray to get full paths.
    CString         fCurrentDirectory;

    // File names of selected files.
    CStringArray    fFileNameArray;
};

#endif /*UTIL_SELECTFILESDIALOG_H*/
