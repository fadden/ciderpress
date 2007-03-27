/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Allow the user to create a new folder.
 */
#include "stdafx.h"
#include "NewFolderDialog.h"

BEGIN_MESSAGE_MAP(NewFolderDialog, CDialog)
	ON_WM_HELPINFO()
END_MESSAGE_MAP()

/*
 * Convert values.
 *
 * It is very important to keep '\\' out of the folder path, because it allows
 * for all sorts of behavior (like "..\foo" or "D:\ack") that the caller
 * might not be expecting.  For example, if it's displaying a tree, it
 * might assume that the folder goes under the currently selected node.
 *
 * Under WinNT, '/' is regarded as equivalent to '\', so we have to block
 * that as well.
 *
 * Other characters (':') are also dangerous, but so long as we start with
 * a valid path, Windows will prevent them from being used where they are
 * inappropriate.
 */
void
NewFolderDialog::DoDataExchange(CDataExchange* pDX)
{
	if (!pDX->m_bSaveAndValidate)
		DDX_Text(pDX, IDC_NEWFOLDER_CURDIR, fCurrentFolder);

	DDX_Text(pDX, IDC_NEWFOLDER_NAME, fNewFolder);

	/* validate the new folder by creating it */
	if (pDX->m_bSaveAndValidate) {
		if (fNewFolder.IsEmpty()) {
			MessageBox("No name entered, not creating new folder.",
				"CiderPress", MB_OK);
			// fall out of DoModal with fFolderCreated==false
		} else if (fNewFolder.Find('\\') >= 0 ||
				   fNewFolder.Find('/') >= 0)
		{
			MessageBox("Folder names may not contain '/' or '\\'.",
				"CiderPress", MB_OK);
			pDX->Fail();
		} else {
			fNewFullPath = fCurrentFolder;
			if (fNewFullPath.Right(1) != "\\")
				fNewFullPath += "\\";
			fNewFullPath += fNewFolder;
			WMSG1("CREATING '%s'\n", fNewFullPath);
			if (!::CreateDirectory(fNewFullPath, nil)) {
				/* show the sometimes-bizarre Windows error string */
				CString msg, errStr, failed;
				DWORD dwerr = ::GetLastError();
				GetWin32ErrorString(dwerr, &errStr);
				msg.Format("Unable to create folder '%s': %s",
					fNewFolder, errStr);
				failed.LoadString(IDS_FAILED);
				MessageBox(msg, failed, MB_OK | MB_ICONERROR);
				pDX->Fail();
			} else {
				/* success! */
				fFolderCreated = true;
			}
		}
	}
}


/*
 * Context help request (question mark button).
 */
BOOL
NewFolderDialog::OnHelpInfo(HELPINFO* lpHelpInfo)
{
	WinHelp((DWORD) lpHelpInfo->iCtrlId, HELP_CONTEXTPOPUP);
	return TRUE;	// yes, we handled it
}
