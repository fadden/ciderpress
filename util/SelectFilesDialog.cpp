/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#include "stdafx.h"
#include "SelectFilesDialog.h"
#include "PathName.h"
#include "Util.h"
#include <dlgs.h>


void SelectFilesDialog2::OnInitDone()
{
    // Tweak the controls
    SetControlText(IDOK, L"Accept");

    HideControl(stc2);     // "Files of type"
    HideControl(cmb1);     // (file type combo)

    // Configure a window proc to intercept events.  We need to do it this
    // way, rather than using m_ofn.lpfnHook, because the CFileDialog hook
    // does not receive messages intended for the standard controls of the
    // default dialog box.  Since our goal is to intercept the "OK" button,
    // we need to set a proc for the whole window.
    CWnd* pParent = GetParent();
    fPrevWndProc = (WNDPROC) ::SetWindowLongPtr(pParent->m_hWnd, GWL_WNDPROC,
        (LONG_PTR) MyWindowProc);

    // Stuff a pointer to this object into the userdata field.
    ::SetWindowLongPtr(pParent->m_hWnd, GWL_USERDATA, (LONG_PTR) this);
}

void SelectFilesDialog2::OnFolderChange()
{
    // We get one of these shortly after OnInitDone.  We can't do this in
    // OnInitDone because the dialog isn't ready yet.
    //
    // Ideally we'd just call GetFolderPath(), but for some reason that
    // returns an empty string for places like Desktop or My Documents
    // (which, unlike Computer or Libraries, do have filesystem paths).
    // You actually get the path in the string returned from the multi-select
    // file dialog, but apparently you have to use a semi-hidden method
    // to get at it from OnFolderChange.
    //
    // In other words, par for the course in Windows file dialogs.

    fCurrentDirectory = L"";
    CWnd* pParent = GetParent();
    LRESULT len = pParent->SendMessage(CDM_GETFOLDERIDLIST, 0, NULL);
    if (len <= 0) {
        LOGW("Can't get folder ID list, len is %d", len);
    } else {
        LPCITEMIDLIST pidlFolder = (LPCITEMIDLIST) CoTaskMemAlloc(len);
        len = pParent->SendMessage(CDM_GETFOLDERIDLIST, len,
                (LPARAM) pidlFolder);
        ASSERT(len > 0);    // should fail earlier, or not at all

        WCHAR buf[MAX_PATH];
        BOOL result = SHGetPathFromIDList(pidlFolder, buf);
        if (result) {
            fCurrentDirectory = buf;
        } else {
            fCurrentDirectory = L"";
        }
        CoTaskMemFree((LPVOID) pidlFolder);
    }

    LOGD("OnFolderChange: '%ls'", (LPCWSTR) fCurrentDirectory);
}

BOOL SelectFilesDialog2::OnFileNameOK()
{
    // This function provides "custom validation of filenames that are
    // entered into a common file dialog box".  We don't need to validate
    // filenames here, but we do require it for another reason: if the user
    // double-clicks a file, the dialog will accept the name and close
    // without our WindowProc getting a WM_COMMAND.
    //
    // This function doesn't get called if we select a file or files and
    // hit the OK button or enter key, because we intercept that before 
    // the dialog can get to it.  It also doesn't get called if you
    // double-click directory, as directory traversal is handled internally
    // by the dialog.
    //
    // It's possible to click then shift-double-click to select a range,
    // so we can have content in both the list and the text box.  This is
    // really just another way to hit "OK".

    LOGD("OnFileNameOK");
    CFileDialog* pDialog = (CFileDialog*) GetParent();
    FPResult fpr = OKButtonClicked(pDialog);
    switch (fpr) {
    case kFPDone:
        return 0;       // success, let the dialog exit
    case kFPPassThru:
        LOGW("WEIRD: got OK_PASSTHRU from double click");
        return 1;
    case kFPNoFiles:
        LOGW("WEIRD: got OK_NOFILES from double click");
        return 1;
    case kFPError:
        return 1;
    default:
        assert(false);
        return 1;
    }
    // not reached
}

/*static*/ LRESULT CALLBACK SelectFilesDialog2::MyWindowProc(HWND hwnd,
    UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SelectFilesDialog2* pSFD =
        (SelectFilesDialog2*) ::GetWindowLong(hwnd, GWL_USERDATA);

    if (uMsg == WM_COMMAND) {
        // React to a click on the OK button (also triggered by hitting return
        // in the filename text box).
        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDOK) {
            // Obtain a CFileDialog pointer from the window handle.  The
            // SelectFilesDialog pointer only gets us to a child window
            // (the one used to implement the templated custom stuff).
            CFileDialog* pDialog = (CFileDialog*) CWnd::FromHandle(hwnd);
            FPResult fpr = pSFD->OKButtonClicked(pDialog);
            switch (fpr) {
            case kFPDone:
                // Success; close the dialog with IDOK as the return.
                LOGD("Calling EndDialog");
                pDialog->EndDialog(IDOK);
                return 0;
            case kFPPassThru:
                LOGD("passing through");
                // User typed a single name that didn't resolve to a
                // simple file.  Let the CFileDialog have it so it can
                // do a directory change.  (We don't just want to return
                // nonzero -- fall through to call prior window proc.)
                break;
            case kFPNoFiles:
                // No files have been selected.  We popped up a message box.
                // Discontinue processing.
                return 0;
            case kFPError:
                // Something failed internally.  Don't let the parent dialog
                // continue on.
                return 0;
            default:
                assert(false);
                return 0;
            }
        }
    }

    return ::CallWindowProc(pSFD->fPrevWndProc, hwnd, uMsg, wParam, lParam);
}

SelectFilesDialog2::FPResult SelectFilesDialog2::OKButtonClicked(CFileDialog* pDialog)
{
    // There are two not-quite-independent sources of filenames.  As
    // ordinary (non-directory) files are selected, they are added to the
    // edit control.  The user is free to edit the text in the box.  This is
    // then what would be returned to the user of the file dialog.  With
    // OFN_FILEMUSTEXIST, the dialog would even confirm that the files exist.
    //
    // It is possible to select a bunch of files, delete the text, type
    // some more names, and hit OK.  In that case you would get a completely
    // disjoint set of names in the list control and edit control.
    //
    // Complicating matters somewhat is the "hide extensions for known file
    // types" feature, which strips the filename extensions from the entries
    // in the list control.  As the files are selected, the names -- with
    // extensions -- are added to the edit box.  So we'd really like to get
    // the names from the edit control.
    //
    // (Win7: Control Panel, Appearance and Personalization, Folder Options,
    // View tab, scroll down, "Hide extensions for known file types" checkbox.)
    //
    // So we need to get the directory names out of the list control, because
    // that's the only place we can find them, and we need to get the file
    // names out of the edit control, because that's the only way to get the
    // full file name (not to mention any names the user typed).
    //
    // We have a special case: we want to be able to type a path into
    // the filename field to go directly there (e.g. "C:\src\test", or a
    // network path like "\\webby\fadden").  If the text edit field contains
    // a single name, and the name isn't a simple file, we want to let the
    // selection dialog exercise its default behavior.

    LOGV("OKButtonClicked");

    // reset array in case we had a previous partially-successful attempt
    fFileNameArray.RemoveAll();

    // add a slash to the directory name, unless it's e.g. "C:\"
    CString curDirSlash = fCurrentDirectory;
    if (curDirSlash.GetAt(curDirSlash.GetLength() - 1) != '\\') {
        curDirSlash += '\\';
    }

    // Get the contents of the text edit control.
    CString editStr;
    LRESULT editLen = pDialog->SendMessage(CDM_GETSPEC, 0, NULL);
    if (editLen > 0) {
        LPTSTR buf = editStr.GetBuffer(editLen);
        pDialog->SendMessage(CDM_GETSPEC, editLen, (LPARAM) buf);
        editStr.ReleaseBuffer();
    }
    LOGV("textedit has '%ls'", (LPCWSTR) editStr);

    // Parse the contents into fFileNameArray.
    int fileCount = ParseFileNames(editStr);
    if (fileCount < 0) {
        ::MessageBeep(MB_ICONERROR);
        return kFPError;
    }

    // find the ShellView control
    CWnd* pListWrapper = pDialog->GetDlgItem(lst2);
    if (pListWrapper == NULL) {
        LOGE("Unable to find ShellView (lst2=%d) in file dialog", lst2);
        return kFPError;
    }

    // get the list control, with voodoo
    CListCtrl* pList = (CListCtrl*) pListWrapper->GetDlgItem(1);
    if (pList == NULL) {
        LOGE("Unable to find list control");
        return kFPError;
    }

    // Check to see if nothing is selected, or exactly one directory has
    // been found in the text box.
    int listCount = pList->GetSelectedCount();
    LOGD("Found %d selected items", listCount);
    if (listCount + fileCount == 0) {
        MessageBox(L"Please select one or more files and directories.",
            m_ofn.lpstrTitle, MB_OK | MB_ICONWARNING);
        return kFPNoFiles;
    } else if (fileCount == 1 && listCount == 0) {
        CString file(fFileNameArray[0]);
        CString path;
        if (file.Find('\\') == -1) {
            // just the filename, prepend the current dir
            path = curDirSlash + file;
        } else {
            // full (?) path, don't alter it
            path = file;
        }
        DWORD attr = GetFileAttributes(path);
        LOGV("Checking to see if '%ls' is a directory: 0x%08lx",
            (LPCWSTR) path, attr);
        if (attr != INVALID_FILE_ATTRIBUTES &&
                (attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            return kFPPassThru;
        }
    }

    // Run through the list, looking for directories.  We have to prepend the
    // current directory to each entry and check the filesystem.
    int index = pList->GetNextItem(-1, LVNI_SELECTED);
    int dirCount = 0;
    while (listCount--) {
        CString itemText(pList->GetItemText(index, 0));
        CString path(curDirSlash + itemText);
        DWORD attr = GetFileAttributes(path);
        LOGV(" %d: 0x%08lx '%ls'", index, attr, (LPCWSTR) itemText);
        if (attr != INVALID_FILE_ATTRIBUTES &&
                (attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            fFileNameArray.Add(itemText);
            dirCount++;
        }

        index = pList->GetNextItem(index, LVNI_SELECTED);
    }
    LOGV(" added %d directories", dirCount);

    return kFPDone;
}

int SelectFilesDialog2::ParseFileNames(const CString& str)
{
    // The filename string can come in two forms.  If only one filename was
    // selected, the entire string will be the filename (spaces and all).  If
    // more than one file was selected, the individual filenames will be
    // surrounded by double quotes.  All of this assumes that the names were
    // put there by the dialog -- if the user just types a bunch of
    // space-separated filenames without quotes, we can't tell that from a
    // single long name with spaces.
    //
    // Double quotes aren't legal in Windows filenames, so we don't have
    // to worry about embedded quotes.
    //
    // So: if the string contains any double quotes, we assume that a series
    // of quoted names follows.  Anything outside quotes is accepted as a
    // single space-separated name.  If the string does not have any '"',
    // we just grab the whole thing as a single entry.
    //
    // It's possible to see a full path here -- we allow a special case
    // where the user can type a path into the text box to change directories.
    // We only expect to see one of those, though, so it must not be quoted.
    //
    // The dialog seems to leave an extra space at the end of multi-file
    // strings.

    LOGD("ParseFileNames '%ls'", (LPCWSTR) str);
    if (str.GetLength() == 0) {
        // empty string
        return 0;
    } else if (str.Find('\"') == -1) {
        // no quotes, single filename, possibly with spaces
        fFileNameArray.Add(str);
        return 1;
    } else if (str.Find('\\') != -1) {
        // should not be multiple full/partial paths, string is invalid
        LOGW("Found multiple paths in '%ls'", (LPCWSTR) str);
        return -1;
    }

    const WCHAR* cp = str;
    const WCHAR* start;
    while (*cp != '\0') {
        // consume whitespace, which should only be spaces
        while (*cp == ' ') {
            cp++;
        }

        if (*cp == '\0') {
            // reached end of string
            break;
        } else if (*cp == '\"') {
            // grab quoted item
            cp++;
            start = cp;
            while (*cp != '\"' && *cp != '\0') {
                cp++;
            }

            if (cp == start) {
                // empty quoted string; ignore
                LOGV("  found empty string in '%ls'", (LPCWSTR) str);
            } else {
                CString name(start, cp - start);
                LOGV("  got quoted '%ls'", (LPCWSTR) name);
                fFileNameArray.Add(name);
            }
            if (*cp != '\0') {
                cp++;  // advance past closing quote
            } else {
                LOGV("   (missing closing quote)");
            }
        } else {
            // found unquoted characters
            start = cp;
            cp++;
            while (*cp != ' ' && *cp != '\"' && *cp != '\0') {
                cp++;
            }
            CString name(start, cp - start);
            LOGV("  got unquoted '%ls'", (LPCWSTR) name);
            fFileNameArray.Add(name);
        }
    }

    return 0;
}













/*
 * Our CFileDialog "hook" function.
 *
 * "hdlg" is a handle to the child dialog box.  Use the GetParent() function
 * to get the handle of the dialog box window.
 *
 * uiMsg identifies the message being received.  If it's WM_INITDIALOG, then
 * lParam points to the OPENFILENAME structure.
 *
 * Do not call EndDialog from here.  Instead, PostMessage a WM_COMMAND with
 * IDABORT.  (Looks like you can EndDialog on the parent and have it work, at
 * least for IDCANCEL.)
 *
 * Return zero to enable standard processing, nonzero to claim ownership of
 * the message.
 */
/*static*/ UINT CALLBACK SelectFilesDialog::OFNHookProc(HWND hDlg, UINT uiMsg,
    WPARAM wParam, LPARAM lParam)
{
    OPENFILENAME* pOfn;
    SelectFilesDialog* pSFD = NULL;
    pOfn = (OPENFILENAME*) GetWindowLong(hDlg, GWL_USERDATA);
    if (pOfn != NULL) {
        pSFD = (SelectFilesDialog*) pOfn->lCustData;
        /* allow our "this" pointer to play with the window */
        /* [does not seem to cause double-frees on cleanup] */
        if (pSFD->m_hWnd == NULL)
            pSFD->m_hWnd = hDlg;
    }

    switch (uiMsg) {
    case WM_INITDIALOG:
        LOGD("WM_INITDIALOG, OFN=0x%08lx", lParam);
        SetWindowLong(hDlg, GWL_USERDATA, lParam);
        break;
    case WM_NOTIFY:     // 0x4e
        ASSERT(pSFD != NULL);
        return pSFD->HandleNotify(hDlg, (LPOFNOTIFY)lParam);
    case WM_COMMAND:
        ASSERT(pSFD != NULL);
        return pSFD->HandleCommand(hDlg, wParam, lParam);
    case WM_SIZE:
        ASSERT(pSFD != NULL);
        return pSFD->HandleSize(hDlg, wParam, LOWORD(lParam), HIWORD(lParam));
    case WM_HELP:
        ASSERT(pSFD != NULL);
        return pSFD->HandleHelp(hDlg, (LPHELPINFO) lParam);
    default:
        LOGV("OFNHookProc: hDlg=0x%08lx uiMsg=0x%08lx "
             "wParam=0x%08lx lParam=0x%08lx",
                hDlg, uiMsg, wParam, lParam);
        break;
    }

    return 0;
}

/*
 * Handle WM_NOTIFY messages.
 *
 * You can indicate displeasure with the CDN_* messages by using SetWindowLong
 * to alter the DWL_MSGRESULT value.
 */
UINT SelectFilesDialog::HandleNotify(HWND hDlg, LPOFNOTIFY pofn)
{
//  int count;

    switch (pofn->hdr.code) {
    case CDN_INITDONE:
        MyOnInitDone();
        return 1;
    case CDN_SELCHANGE:
        LOGI("  CDN_SELCHANGE");
        MyOnFileNameChange(/*&count*/);
        //ClearFileName();
        return 1;
    case CDN_FOLDERCHANGE:
        LOGI("  CDN_FOLDERCHANGE");
        break;
    case CDN_SHAREVIOLATION:
        LOGI("  CDN_SHAREVIOLATION");
        break;
    case CDN_HELP:
        LOGI("  CDN_HELP!");
        break;
    case CDN_FILEOK:
        LOGI("  CDN_FILEOK");
        /* act like they hit the Accept button */
//      MyOnFileNameChange(&count);
        //ClearFileName();
//      if (count != 0) {
//          LOGI("Count = %d, accepting CDN_FILEOK", count);
//          MyOnAccept();
//      } else {
//          OPENFILENAME* pOfn;
//          pOfn = (OPENFILENAME*) GetWindowLong(hDlg, GWL_USERDATA);
//          LOGI("Count=0, name='%ls'", pOfn->lpstrFile);
//      }
        PrepEndDialog();
        /* must do this every time, or it fails in funky ways */
        SetWindowLong(hDlg, DWL_MSGRESULT, 1);
        return 1;
    case CDN_TYPECHANGE:
        LOGI("  CDN_TYPECHANGE");
        break;
    case CDN_INCLUDEITEM:
        LOGI("  CDN_INCLUDEITEM");
    default:
        LOGI("  HandleNotify, code=%d, pOfn=0x%p", pofn->hdr.code, pofn);
        break;
    }

    return 0;
}

/*
 * Handle WM_COMMAND messages.
 */
UINT SelectFilesDialog::HandleCommand(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
    LOGD("  HandleCommand wParam=%d lParam=0x%08lx", wParam, lParam);

    if ((int) wParam == fAcceptButtonID) {
        MyOnAccept();
        return 1;
    } else if (wParam == IDCANCEL) {
        MyOnCancel();
        return 1;
    } else {
        return MyOnCommand(wParam, lParam);
    }
}

/*
 * Handle WM_SIZE.
 */
UINT SelectFilesDialog::HandleSize(HWND hDlg, UINT nType, int cx, int cy)
{
    LOGD("Dialog: old size %d,%d  (ready=%d)",
      fLastWinSize.Width(), fLastWinSize.Height(), fReady);
    LOGD("Dialog: new size %d,%d", cx, cy);

    // we get called once before we have a chance to initialize
    if (!fReady)
        return 0;

    int deltaX, deltaY;
    deltaX = cx - fLastWinSize.Width();
    deltaY = cy - fLastWinSize.Height();
    LOGD("Delta is %d,%d", deltaX, deltaY);

    ShiftControls(deltaX, 0 /*deltaY*/);

    // TODO: this is wrong
    GetParent()->GetWindowRect(&fLastWinSize);

    return 1;
}

/*
 * User hit F1 or applied the '?' button to something.  Our heritage is
 * dubious, so use global functions to access the help file.
 */
UINT SelectFilesDialog::HandleHelp(HWND hDlg, LPHELPINFO lpHelpInfo)
{
    CWnd* pWndMain = ::AfxGetMainWnd();
    CWinApp* pAppMain = ::AfxGetApp();
    DWORD context = lpHelpInfo->iCtrlId;
    BOOL result;

    //LOGI("Handling help with context %ld", context);
    result = ::WinHelp(pWndMain->m_hWnd, pAppMain->m_pszHelpFilePath,
                HELP_CONTEXTPOPUP, context);
    //LOGI("SFD WinHelp returned %d", result);
    return TRUE;    // yes, we handled it
}

/*
 * When the CFileDialog finishes doing its thing, we "fix" stuff a bit.
 * We can't really do this earlier, because we'd be destroying windows that
 * the parent dialog wants to move.
 *
 * We need to shift everything up by the difference between the IDOK button
 * and our "accept" button.
 */
void SelectFilesDialog::MyOnInitDone(void)
{
    LOGI("OnInitDone!");
    CWnd* pParent = GetParent();
    CWnd* pWnd;
    CRect okRect, cancelRect, acceptRect;
    int vertDiff;

    ASSERT(pParent != NULL);
    pWnd = GetDlgItem(fAcceptButtonID);
    ASSERT(pWnd != NULL);
    pWnd->GetWindowRect(&acceptRect);

    pWnd = pParent->GetDlgItem(IDOK);
    ASSERT(pWnd != NULL);
    pWnd->GetWindowRect(&okRect);
    pWnd = pParent->GetDlgItem(IDCANCEL);
    ASSERT(pWnd != NULL);
    pWnd->GetWindowRect(&cancelRect);

    vertDiff = acceptRect.top - okRect.top;
    LOGD("vertDiff = %d (horizDiff=%d)", vertDiff,
        acceptRect.left - okRect.left);

    ShiftControls(0, -vertDiff);

//  DestroyItem(pParent, stc3);     // "File name"
//  DestroyItem(pParent, edt1);     // (file name edit)
    DestroyItem(pParent, stc2);     // "Files of type"
    DestroyItem(pParent, cmb1);     // (file type combo)
    DestroyItem(pParent, IDOK);     // "Open"/"Save"
    DestroyItem(pParent, IDCANCEL); // "Cancel"
    DestroyItem(this, stc32);       // our placeholder

    pParent->GetWindowRect(&fLastWinSize);
    fLastWinSize.bottom -= vertDiff;
    pParent->MoveWindow(&fLastWinSize);

    // let sub-classes initialize the data fields
    MyDataExchange(false);

    fReady = true;
}

/*
 * Shift the controls when the window size changes.  This is a bit tricky
 * because the CFileDialog is also moving the controls, though it doesn't
 * move them in quite the way we want.
 */
void SelectFilesDialog::ShiftControls(int deltaX, int deltaY)
{
    if (deltaX == 0 && deltaY == 0) {
        LOGI("SFD OnSize: no meaningful change");
        return;
    } else {
        LOGI("ShiftControls x=%d y=%d", deltaX, deltaY);
    }
    MoveControl(this, fAcceptButtonID, deltaX, deltaY, false);
    MoveControl(this, IDCANCEL, deltaX, deltaY, false);
    //StretchControl(this, IDC_FVIEW_EDITBOX, deltaX, deltaY);

    // erase & redraw
    Invalidate(true);
}

/*
 * Get the list view control out of the common file dialog.
 *
 * Returns "NULL" if it can't find it.
 */
CWnd* SelectFilesDialog::GetListCtrl(void)
{
    CWnd* pItem;
    CWnd* pList;

    /* our dialog is a child; get our parent, then grab the shellview */
    pItem = GetParent()->GetDlgItem(lst2);
    ASSERT(pItem != NULL);
    if (pItem == NULL)
        return NULL;

    /* pull the listview out of the shellview */
    pList = pItem->GetDlgItem( 1);
    ASSERT(pList != NULL);

    return pList;
}

/*
 * When the selection changes, update our dialog.
 */
void SelectFilesDialog::MyOnFileNameChange(void)
{
    //LOGI("OnFileNameChange");

    CListCtrl* pList;

    pList = (CListCtrl*) GetListCtrl();
    if (pList == NULL) {
        LOGI("GLITCH: could not get list control");
        return;
    }
    ASSERT(pList != NULL);

    //LOGI("Selected count=%d", pList->GetSelectedCount());
    //*pCount = pList->GetSelectedCount();

    //CWnd* pItem;
    //pItem = GetDlgItem(IDC_SELECT_ACCEPT);
    //ASSERT(pItem != NULL);
    //pItem->EnableWindow(*pCount != 0);
}

/*
 * The user hit the "Accept" button.  Package up the file selection.
 */
void SelectFilesDialog::MyOnAccept(void)
{
    LOGD("OnAccept!");
    PrepEndDialog();
}

/*
 * Either the user hit the "Accept" button or the OFN dialog has indicated
 * that it wants to close.
 *
 * Returns "true" if all went well, "false" if it failed (e.g. because the
 * user hasn't selected any files).
 */
bool SelectFilesDialog::PrepEndDialog(void)
{
    CListCtrl* pList;
    int nextSpot = 0;

    // let sub-classes copy data out
    if (!MyDataExchange(true)) {
        LOGW("MyDataExchange failed!");
        return false;
    }

    /*
     * Start with the set of stuff that the window wants to tell us about.
     * Run through the list, converting all '\0' to '\\'.  Later on we
     * convert them back.
     *
     * nFileOffset will be zero if they click on "accept" instead of hitting
     * return in the edit box or double-clicking on files.  This can make it
     * tricky to have names in the text field and files selected, because
     * clicking rather than hitting "enter" will yield different results.
     *
     * The OFN dialog does add names to the box as they are selected, which
     * makes it awkward to do anything reasonable.
     *
     * Fortunately I believe the world is divided into "typers" and
     * "clickers", and so long as their paths don't cross we're fine.
     */
    LOGD("PrepEndDialog: got max=%d off=%d str='%ls'",
        m_ofn.nMaxFile, m_ofn.nFileOffset, m_ofn.lpstrFile);
    if (m_ofn.nFileOffset != 0) {
        WCHAR* buf = m_ofn.lpstrFile;
        buf += m_ofn.nFileOffset;
        while (*buf != '\0') {
            if (buf > m_ofn.lpstrFile)
                *(buf-1) = '\\';
            LOGD("    File '%ls'", buf);
            buf += wcslen(buf) +1;
        }
        //Sleep(1000);
        nextSpot = (buf - m_ofn.lpstrFile) -1;
        ASSERT(*(m_ofn.lpstrFile + nextSpot) == '\0');
        ASSERT(*(m_ofn.lpstrFile + nextSpot+1) == '\0');

        /* stick a '\' on the very end, so we get double-null action later */
        *(m_ofn.lpstrFile + nextSpot) = '\\';
    }
    LOGD("Last offset was %d", nextSpot);

#if 0
    /* make it clear that they're only getting one */
    /* (bad case: click on some files and hit "Accept") */
    if (nextSpot == 0) {
        CWnd* pEditWnd;
        CString editText;
        pEditWnd = GetParent()->GetDlgItem(edt1);
        pEditWnd->GetWindowText(editText);
        if (!editText.IsEmpty()) {
            pEditWnd->SetWindowText("");
            return false;
        }
    }
#endif

    /*
     * Now merge in the selected files.
     */
    pList = (CListCtrl*) GetListCtrl();
    if (pList == NULL) {
        LOGW("GLITCH: could not get list control");
        return false;
    }
    ASSERT(pList != NULL);

    CString fileNames;

    int count = pList->GetSelectedCount();
    LOGD("List control has %d items; nextSpot=%d", count, nextSpot);
    if (count == 0) {
        if (nextSpot == 0) {
            MessageBox(L"Please select one or more files and directories.",
                m_ofn.lpstrTitle, MB_OK | MB_ICONWARNING);
            /* make it clear that we're ignoring the names they typed */
            ClearFileName();
            return false;
        }

        /* nothing but typed-in names */
        LOGD("Using typed-in names");
        fileNames = m_ofn.lpstrFile;
        fFileNameOffset = m_ofn.nFileOffset;
    } else {
        bool compare;
        if (nextSpot == 0) {
            fileNames = GetFolderPath();
            LOGD("set filenames to folder path (%ls)", (LPCWSTR) fileNames);
            /* add a trailing '\', which gets stomped to '\0' later on */
            if (fileNames.Right(1) != L"\\")
                fileNames += L"\\";
            fFileNameOffset = fileNames.GetLength();
            compare = false;
        } else {
            fileNames = m_ofn.lpstrFile;
            ASSERT(fileNames.Right(1) == L"\\");
            fFileNameOffset = m_ofn.nFileOffset;
            compare = true;
        }
        ASSERT(fFileNameOffset > 0);

        POSITION posn;
        posn = pList->GetFirstSelectedItemPosition();
        if (posn == NULL) {
            /* shouldn't happen -- Accept button should be dimmed */
            ASSERT(false);
            return false;
        }
        while (posn != NULL) {
            /* do this every time, because "fileNames" can be reallocated */
            const WCHAR* tailStr = fileNames;
            tailStr += fFileNameOffset-1;

            int num = pList->GetNextSelectedItem(posn);     // posn is updated

            /* here we make a big assumption: that GetItemData returns a PIDL */
            LPITEMIDLIST pidl;
            WCHAR buf[MAX_PATH];
            pidl = (LPITEMIDLIST) pList->GetItemData(num);
            if (SHGetPathFromIDList(pidl, buf)) {
                /* it's a relative PIDL but SHGetPathFromIDList wants a full
                   one, so it returns CWD + filename... strip bogus path off */
                CString compareName(L"\\");
                PathName path(buf);
                compareName += path.GetFileName();
                compareName += L"\\";
                //LOGI("  Checking name='%ls'", compareName);

                if (compare && Stristr(tailStr, compareName) != NULL) {
                    LOGI("    Matched '%ls', not adding", (LPCWSTR) compareName);
                } else {
                    if (compare) {
                        LOGI("    No match on '%ls', adding", (LPCWSTR) compareName);
                    } else {
                        LOGI("    Found '%ls', adding", (LPCWSTR) compareName);
                    }
                    fileNames += path.GetFileName();
                    fileNames += L"\\";
                }
            } else {
                /* expected, for things like "Control Panels" or "My Network" */
                LOGI("  No path for '%ls'",
                    (LPCWSTR) pList->GetItemText(num, 0));
            }
        }

        if (fileNames.GetLength() >= (int)m_ofn.nMaxFile) {
            LOGW("GLITCH: excessively long file name list");
            return false;
        }
    }

    LOGI("Final result: names at %d, len=%d, str='%ls'",
        fFileNameOffset, wcslen(fileNames), (LPCWSTR) fileNames);

    /*
     * Null-terminate with extreme prejudice.  Every filename should be
     * separated with a null, and the last filename should be followed by
     * two.  Every component was followed by '\\', and the last one
     * naturally has a null, so we should be in great shape after this.
     *
     * Could probably have done it entirely with CString, but I'm not sure
     * how CStrings react to buffers with multiple null-terminated
     * sub-strings.
     */
    ASSERT(fFileNames != m_ofn.lpstrFile);
    delete[] fFileNames;
    fFileNames = wcsdup(fileNames);
    WCHAR* cp = fFileNames;
    cp += fFileNameOffset-1;
    while (*cp != '\0') {
        if (*cp == '\\')
            *cp = '\0';
        cp++;
    }
    //fileNames.ReleaseBuffer(-1);

    fExitStatus = IDOK;
    CDialog* pDialog = (CDialog*) GetParent();
    pDialog->EndDialog(IDCANCEL);

    return true;
}

/*
 * User hit our cancel button.
 */
void SelectFilesDialog::MyOnCancel(void)
{
    fExitStatus = IDCANCEL;
    CDialog* pDialog = (CDialog*) GetParent();
    pDialog->EndDialog(IDCANCEL);
}

/*
 * Clear the filename field.
 */
void SelectFilesDialog::ClearFileName(void)
{
    CWnd* pWnd = GetParent()->GetDlgItem(edt1);
    if (pWnd != NULL)
        pWnd->SetWindowText(L"");
}
