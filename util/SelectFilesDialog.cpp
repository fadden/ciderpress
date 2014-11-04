/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for file selection dialog.
 *
 * What we want to do is get the full set of selected files out of the dialog.
 * However, Windows does not provide a way to do that.  There are various
 * slightly awkward approaches:
 *
 *  - Get the IShellView pointer via the IShellBrowser pointer via the
 *    undocumented WM_GETISHELLBROWSER message.  You can then play with the
 *    shellview window, select items with SelectItem(), and get an object
 *    representing the selected items with another call.  What form the
 *    object takes is frustratingly unspecified.
 *    http://www.codeproject.com/dialog/select_all_button.asp
 *  - Get the CListView so you can play with its members.  This is done by
 *    GetParent --> GetItem(lst2) --> GetItem(1), which is probably
 *    somewhat fragile, but seems to work on 98 through XP.  Standard LVN
 *    stuff applies.
 *    http://www.codeguru.com/mfc/comments/10216.shtml
 *  - Using a window hook (not an OFN hook), get the class name and strcmp()
 *    it for the class we're looking for ("syslistview32").  Once we have
 *    it, proceed as above.
 *    http://www.codeproject.com/dialog/customize_dialog.asp
 *
 * Care must be taken with the ListView because it doesn't contain file names,
 * but rather display names (potentially without filename extensions).  The
 * next big assumptive leap is that the ItemData pointer is a shell PIDL that
 * can be used directly.
 *
 * The PIDL stored is, of course, relative to the IShellFolder currently being
 * displayed.  There's no easy way to get that, but if we just go ahead and
 * call SHGetPathFromIDList anyway we get the right file name with the wrong
 * path (it appears to be the desktop folder).  We can strip the path off and
 * prepend the value from the undocumented GetFolderPath() call (which just
 * issues a CDM_GETFOLDERPATH message).
 *
 * To make matters even more interesting, it is necessary to provide a "hook"
 * function to prevent double-clicking from closing the dialog while side-
 * stepping our additions.  Of course, adding an OFN hook renders all of the
 * existing message map and initialization stuff inoperable.  You can stuff
 * things through various "user data" pointers and end up with access to your
 * object, and if you cram the hook procedure's hDlg into the object's m_hWnd
 * you can treat "this" like a window instead of passing HWNDs around.
 */
#include "stdafx.h"
#include "SelectFilesDialog.h"
#include "PathName.h"
#include "Util.h"
#include <dlgs.h>


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
/*static*/ UINT CALLBACK
SelectFilesDialog::OFNHookProc(HWND hDlg, UINT uiMsg, WPARAM wParam,
    LPARAM lParam)
{
    OPENFILENAME* pOfn;
    SelectFilesDialog* pSFD = nil;
    pOfn = (OPENFILENAME*) GetWindowLong(hDlg, GWL_USERDATA);
    if (pOfn != nil) {
        pSFD = (SelectFilesDialog*) pOfn->lCustData;
        /* allow our "this" pointer to play with the window */
        /* [does not seem to cause double-frees on cleanup] */
        if (pSFD->m_hWnd == nil)
            pSFD->m_hWnd = hDlg;
    }

    switch (uiMsg) {
    case WM_INITDIALOG:
        WMSG1("WM_INITDIALOG, OFN=0x%08lx\n", lParam);
        SetWindowLong(hDlg, GWL_USERDATA, lParam);
        break;
    case WM_NOTIFY:     // 0x4e
        ASSERT(pSFD != nil);
        return pSFD->HandleNotify(hDlg, (LPOFNOTIFY)lParam);
    case WM_COMMAND:
        ASSERT(pSFD != nil);
        return pSFD->HandleCommand(hDlg, wParam, lParam);
    case WM_SIZE:
        ASSERT(pSFD != nil);
        return pSFD->HandleSize(hDlg, wParam, LOWORD(lParam), HIWORD(lParam));
    case WM_HELP:
        ASSERT(pSFD != nil);
        return pSFD->HandleHelp(hDlg, (LPHELPINFO) lParam);
    default:
        //WMSG4("OFNHookProc: hDlg=0x%08lx uiMsg=0x%08lx "
        //      "wParam=0x%08lx lParam=0x%08lx\n",
        //  hDlg, uiMsg, wParam, lParam);
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
UINT
SelectFilesDialog::HandleNotify(HWND hDlg, LPOFNOTIFY pofn)
{
//  int count;

    switch (pofn->hdr.code) {
    case CDN_INITDONE:
        MyOnInitDone();
        return 1;
    case CDN_SELCHANGE:
        WMSG0("  CDN_SELCHANGE\n");
        MyOnFileNameChange(/*&count*/);
        //ClearFileName();
        return 1;
    case CDN_FOLDERCHANGE:
        WMSG0("  CDN_FOLDERCHANGE\n");
        break;
    case CDN_SHAREVIOLATION:
        WMSG0("  CDN_SHAREVIOLATION\n");
        break;
    case CDN_HELP:
        WMSG0("  CDN_HELP!\n");
        break;
    case CDN_FILEOK:
        WMSG0("  CDN_FILEOK\n");
        /* act like they hit the Accept button */
//      MyOnFileNameChange(&count);
        //ClearFileName();
//      if (count != 0) {
//          WMSG1("Count = %d, accepting CDN_FILEOK\n", count);
//          MyOnAccept();
//      } else {
//          OPENFILENAME* pOfn;
//          pOfn = (OPENFILENAME*) GetWindowLong(hDlg, GWL_USERDATA);
//          WMSG1("Count=0, name='%s'\n", pOfn->lpstrFile);
//      }
        PrepEndDialog();
        /* must do this every time, or it fails in funky ways */
        SetWindowLong(hDlg, DWL_MSGRESULT, 1);
        return 1;
    case CDN_TYPECHANGE:
        WMSG0("  CDN_TYPECHANGE\n");
        break;
    case CDN_INCLUDEITEM:
        WMSG0("  CDN_INCLUDEITEM\n");
    default:
        WMSG2("  HandleNotify, code=%d, pOfn=0x%08lx\n", pofn->hdr.code, pofn);
        break;
    }

    return 0;
}

/*
 * Handle WM_COMMAND messages.
 */
UINT
SelectFilesDialog::HandleCommand(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
    WMSG2("  HandleCommand wParam=%d lParam=0x%08lx\n", wParam, lParam);

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
UINT
SelectFilesDialog::HandleSize(HWND hDlg, UINT nType, int cx, int cy)
{
    //WMSG3("Dialog: old size %d,%d  (ready=%d)\n",
    //  fLastWinSize.Width(), fLastWinSize.Height(), fReady);
    //WMSG2("Dialog: new size %d,%d\n", cx, cy);

    // we get called once before we have a chance to initialize
    if (!fReady)
        return 0;

    int deltaX, deltaY;
    deltaX = cx - fLastWinSize.Width();
    deltaY = cy - fLastWinSize.Height();
    //WMSG2("Delta is %d,%d\n", deltaX, deltaY);

    ShiftControls(deltaX, 0 /*deltaY*/);

    GetParent()->GetWindowRect(&fLastWinSize);

    return 1;
}

/*
 * User hit F1 or applied the '?' button to something.  Our heritage is
 * dubious, so use global functions to access the help file.
 */
UINT
SelectFilesDialog::HandleHelp(HWND hDlg, LPHELPINFO lpHelpInfo)
{
    CWnd* pWndMain = ::AfxGetMainWnd();
    CWinApp* pAppMain = ::AfxGetApp();
    DWORD context = lpHelpInfo->iCtrlId;
    BOOL result;

    //WMSG1("Handling help with context %ld\n", context);
    result = ::WinHelp(pWndMain->m_hWnd, pAppMain->m_pszHelpFilePath,
                HELP_CONTEXTPOPUP, context);
    //WMSG1("SFD WinHelp returned %d\n", result);
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
void
SelectFilesDialog::MyOnInitDone(void)
{
    WMSG0("OnInitDone!\n");
    CWnd* pParent = GetParent();
    CWnd* pWnd;
    CRect okRect, cancelRect, acceptRect;
    int vertDiff;

    ASSERT(pParent != nil);
    pWnd = GetDlgItem(fAcceptButtonID);
    ASSERT(pWnd != nil);
    pWnd->GetWindowRect(&acceptRect);

    pWnd = pParent->GetDlgItem(IDOK);
    ASSERT(pWnd != nil);
    pWnd->GetWindowRect(&okRect);
    pWnd = pParent->GetDlgItem(IDCANCEL);
    ASSERT(pWnd != nil);
    pWnd->GetWindowRect(&cancelRect);

    vertDiff = acceptRect.top - okRect.top;
    WMSG2("vertDiff = %d (horizDiff=%d)\n", vertDiff,
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
void
SelectFilesDialog::ShiftControls(int deltaX, int deltaY)
{
    if (deltaX == 0 && deltaY == 0) {
        WMSG0("SFD OnSize: no meaningful change\n");
        return;
    } else {
        WMSG2("ShiftControls x=%d y=%d\n", deltaX, deltaY);
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
 * Returns "nil" if it can't find it.
 */
CWnd*
SelectFilesDialog::GetListCtrl(void)
{
    CWnd* pItem;
    CWnd* pList;

    /* our dialog is a child; get our parent, then grab the shellview */
    pItem = GetParent()->GetDlgItem(lst2);
    ASSERT(pItem != nil);
    if (pItem == nil)
        return nil;

    /* pull the listview out of the shellview */
    pList = pItem->GetDlgItem( 1);
    ASSERT(pList != nil);

    return pList;
}

/*
 * When the selection changes, update our dialog.
 */
void
SelectFilesDialog::MyOnFileNameChange(void)
{
    //WMSG1("OnFileNameChange\n");

    CListCtrl* pList;

    pList = (CListCtrl*) GetListCtrl();
    if (pList == nil) {
        WMSG0("GLITCH: could not get list control\n");
        return;
    }
    ASSERT(pList != nil);

    //WMSG1("Selected count=%d\n", pList->GetSelectedCount());
    //*pCount = pList->GetSelectedCount();

    //CWnd* pItem;
    //pItem = GetDlgItem(IDC_SELECT_ACCEPT);
    //ASSERT(pItem != nil);
    //pItem->EnableWindow(*pCount != 0);
}

/*
 * The user hit the "Accept" button.  Package up the file selection.
 */
void
SelectFilesDialog::MyOnAccept(void)
{
    //WMSG0("OnAccept!\n");
    PrepEndDialog();
}

/*
 * Either the user hit the "Accept" button or the OFN dialog has indicated
 * that it wants to close.
 *
 * Returns "true" if all went well, "false" if it failed (e.g. because the
 * user hasn't selected any files).
 */
bool
SelectFilesDialog::PrepEndDialog(void)
{
    CListCtrl* pList;
    int nextSpot = 0;

    // let sub-classes copy data out
    if (!MyDataExchange(true)) {
        WMSG0("MyDataExchange failed!\n");
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
    WMSG2("PrepEndDialog: got max=%d off=%d\n", m_ofn.nMaxFile, m_ofn.nFileOffset);
    if (m_ofn.nFileOffset != 0) {
        char* buf = m_ofn.lpstrFile;
        buf += m_ofn.nFileOffset;
        while (*buf != '\0') {
            if (buf > m_ofn.lpstrFile)
                *(buf-1) = '\\';
            WMSG1("    File '%s'\n", buf);
            buf += strlen(buf) +1;
        }
        //Sleep(1000);
        nextSpot = (buf - m_ofn.lpstrFile) -1;
        ASSERT(*(m_ofn.lpstrFile + nextSpot) == '\0');
        ASSERT(*(m_ofn.lpstrFile + nextSpot+1) == '\0');

        /* stick a '\' on the very end, so we get double-null action later */
        *(m_ofn.lpstrFile + nextSpot) = '\\';
    }
    WMSG1("Last offset was %d\n", nextSpot);

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
    if (pList == nil) {
        WMSG0("GLITCH: could not get list control\n");
        return false;
    }
    ASSERT(pList != nil);

    CString fileNames;

    int count = pList->GetSelectedCount();
    if (count == 0) {
        if (nextSpot == 0) {
            MessageBox("Please select one or more files and directories.",
                m_ofn.lpstrTitle, MB_OK | MB_ICONWARNING);
            /* make it clear that we're ignoring the names they typed */
            ClearFileName();
            return false;
        }

        /* nothing but typed-in names */
        fileNames = m_ofn.lpstrFile;
        fFileNameOffset = m_ofn.nFileOffset;
    } else {
        bool compare;
        if (nextSpot == 0) {
            fileNames = GetFolderPath();
            /* add a trailing '\', which gets stomped to '\0' later on */
            if (fileNames.Right(1) != "\\")
                fileNames += "\\";
            fFileNameOffset = fileNames.GetLength();
            compare = false;
        } else {
            fileNames = m_ofn.lpstrFile;
            ASSERT(fileNames.Right(1) == "\\");
            fFileNameOffset = m_ofn.nFileOffset;
            compare = true;
        }
        ASSERT(fFileNameOffset > 0);

        POSITION posn;
        posn = pList->GetFirstSelectedItemPosition();
        if (posn == nil) {
            /* shouldn't happen -- Accept button should be dimmed */
            ASSERT(false);
            return false;
        }
        while (posn != nil) {
            /* do this every time, because "fileNames" can be reallocated */
            const char* tailStr = fileNames;
            tailStr += fFileNameOffset-1;

            int num = pList->GetNextSelectedItem(posn);     // posn is updated

            /* here we make a big assumption: that GetItemData returns a PIDL */
            LPITEMIDLIST pidl;
            char buf[MAX_PATH];
            pidl = (LPITEMIDLIST) pList->GetItemData(num);
            if (SHGetPathFromIDList(pidl, buf)) {
                /* it's a relative PIDL but SHGetPathFromIDList wants a full
                   one, so it returns CWD + filename... strip bogus path off */
                CString compareName("\\");
                PathName path(buf);
                compareName += path.GetFileName();
                compareName += "\\";
                //WMSG1("  Checking name='%s'\n", compareName);

                if (compare && stristr(tailStr, compareName) != nil) {
                    WMSG1("    Matched '%s', not adding\n", compareName);
                } else {
                    if (compare) {
                        WMSG1("    No match on '%s', adding\n", compareName);
                    } else {
                        WMSG1("    Found '%s', adding\n", compareName);
                    }
                    fileNames += path.GetFileName();
                    fileNames += "\\";
                }
            } else {
                /* expected, for things like "Control Panels" or "My Network" */
                WMSG1("  No path for '%s'\n",
                    (LPCTSTR) pList->GetItemText(num, 0));
            }
        }

        if (fileNames.GetLength() >= (int)m_ofn.nMaxFile) {
            WMSG0("GLITCH: excessively long file name list\n");
            return false;
        }
    }

    WMSG3("Final result: names at %d, len=%d, str='%s'\n",
        fFileNameOffset, strlen(fileNames), fileNames);

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
    fFileNames = strdup(fileNames);
    char* cp = fFileNames;
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
void
SelectFilesDialog::MyOnCancel(void)
{
    fExitStatus = IDCANCEL;
    CDialog* pDialog = (CDialog*) GetParent();
    pDialog->EndDialog(IDCANCEL);
}

/*
 * Clear the filename field.
 */
void
SelectFilesDialog::ClearFileName(void)
{
    CWnd* pWnd = GetParent()->GetDlgItem(edt1);
    if (pWnd != nil)
        pWnd->SetWindowText("");
}
