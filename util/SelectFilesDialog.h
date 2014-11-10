/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * File selection dialog, a sub-class of "Open" that allows multiple selection
 * of both files and directories.
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
        CFileDialog(true, NULL, NULL, OFN_HIDEREADONLY, NULL, pParentWnd)
    {
        m_ofn.Flags |= OFN_ENABLETEMPLATE | OFN_ALLOWMULTISELECT |
            OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;
        m_ofn.lpTemplateName = rctmpl;
        m_ofn.hInstance = AfxGetInstanceHandle();
        m_ofn.lpstrFile = new WCHAR[kFileNameBufSize];
        m_ofn.lpstrFile[0] = m_ofn.lpstrFile[1] = '\0';
        m_ofn.nMaxFile = kFileNameBufSize;
        m_ofn.Flags |= OFN_ENABLEHOOK;
        m_ofn.lpfnHook = OFNHookProc;
        m_ofn.lCustData = (long)this;

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
        WMSG3("SetFileNames '%ls' %d %d\n", fileNames, len, fileNameOffset);
        delete[] fFileNames;
        fFileNames = wcsdup(fileNames);
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
        if (pWnd == nil) {
            WMSG1("Could not find item %d\n", id);
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

#endif /*UTIL_SELECTFILESDIALOG_H*/
