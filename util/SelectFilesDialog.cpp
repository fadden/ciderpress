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


void SelectFilesDialog::OnInitDone()
{
    // Tweak the controls
    SetControlText(IDOK, L"Accept");

    // we don't take a "files of type" arg, so there's nothing in this combo box
    HideControl(stc2);     // "Files of type"
    HideControl(cmb1);     // (file type combo)

    // Let the subclass do control configuration and data exchange.
    (void) MyDataExchange(false);

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

void SelectFilesDialog::OnFolderChange()
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
        if (pidlFolder == NULL) {
            LOGE("Unable to allocate %d bytes", len);
            return;
        }
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

BOOL SelectFilesDialog::OnFileNameOK()
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

/*static*/ LRESULT CALLBACK SelectFilesDialog::MyWindowProc(HWND hwnd,
    UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SelectFilesDialog* pSFD =
        (SelectFilesDialog*) ::GetWindowLong(hwnd, GWL_USERDATA);

    if (uMsg == WM_COMMAND) {
        // React to a click on the OK button (also triggered by hitting return
        // in the filename text box).
        if (HIWORD(wParam) == BN_CLICKED) {
            if (LOWORD(wParam) == IDOK) {
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
            } else if (LOWORD(wParam) == pshHelp) {
                pSFD->HandleHelp();
                return 0;   // default handler will post "unable to open help"
            }
        }
    }

    return ::CallWindowProc(pSFD->fPrevWndProc, hwnd, uMsg, wParam, lParam);
}

SelectFilesDialog::FPResult SelectFilesDialog::OKButtonClicked(CFileDialog* pDialog)
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

    // let sub-classes copy data out, and have an opportunity to reject values
    if (!MyDataExchange(true)) {
        LOGW("MyDataExchange failed!");
        return kFPError;
    }

    return kFPDone;
}

int SelectFilesDialog::ParseFileNames(const CString& str)
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
