/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of our About box.
 */
#include "stdafx.h"
#include "AboutDialog.h"
#include "EnterRegDialog.h"
#include "HelpTopics.h"
#include "MyApp.h"
#include "resource.h"
#include "../prebuilt/NufxLib.h"
#include "../diskimg/DiskImg.h"
#define ZLIB_DLL
#include "../prebuilt/zlib.h"


BEGIN_MESSAGE_MAP(AboutDialog, CDialog)
	ON_BN_CLICKED(IDC_ABOUT_CREDITS, OnAboutCredits)
	//ON_BN_CLICKED(IDC_ABOUT_ENTER_REG, OnEnterReg)
END_MESSAGE_MAP()

static const char* kVersionExtra =
#ifdef _DEBUG
		" _DEBUG"
#else
		""
#endif
#ifdef _DEBUG_LOG
		" _LOG"
#else
		""
#endif
		;

/*
 * Update the static strings with DLL version numbers.
 */
BOOL
AboutDialog::OnInitDialog(void)
{
	NuError nerr;
	long major, minor, bug;
	CString newVersion, tmpStr;
	CStatic* pStatic;
	//CString versionFmt;

	/* CiderPress version string */
	pStatic = (CStatic*) GetDlgItem(IDC_CIDERPRESS_VERS_TEXT);
	ASSERT(pStatic != nil);
	pStatic->GetWindowText(tmpStr);
	newVersion.Format(tmpStr,
		kAppMajorVersion, kAppMinorVersion, kAppBugVersion,
		kAppDevString, kVersionExtra);
	pStatic->SetWindowText(newVersion);

	/* grab the static text control with the NufxLib version info */
	pStatic = (CStatic*) GetDlgItem(IDC_NUFXLIB_VERS_TEXT);
	ASSERT(pStatic != nil);
	nerr = NuGetVersion(&major, &minor, &bug, NULL, NULL);
	ASSERT(nerr == kNuErrNone);

	pStatic->GetWindowText(tmpStr);
	newVersion.Format(tmpStr, major, minor, bug);
	pStatic->SetWindowText(newVersion);

	/* grab the static text control with the DiskImg version info */
	pStatic = (CStatic*) GetDlgItem(IDC_DISKIMG_VERS_TEXT);
	ASSERT(pStatic != nil);
	DiskImgLib::Global::GetVersion(&major, &minor, &bug);

	pStatic->GetWindowText(tmpStr);
	newVersion.Format(tmpStr, major, minor, bug);
	pStatic->SetWindowText(newVersion);

	/* set the zlib version */
	pStatic = (CStatic*) GetDlgItem(IDC_ZLIB_VERS_TEXT);
	ASSERT(pStatic != nil);
	pStatic->GetWindowText(tmpStr);
	newVersion.Format(tmpStr, zlibVersion());
	pStatic->SetWindowText(newVersion);

	/* and, finally, the ASPI version */
	pStatic = (CStatic*) GetDlgItem(IDC_ASPI_VERS_TEXT);
	ASSERT(pStatic != nil);
	if (DiskImgLib::Global::GetHasASPI()) {
		CString versionStr;
		DWORD version = DiskImgLib::Global::GetASPIVersion();
		versionStr.Format("%d.%d.%d.%d",
			version & 0x0ff,
			(version >> 8) & 0xff,
			(version >> 16) & 0xff,
			(version >> 24) & 0xff);
		pStatic->GetWindowText(tmpStr);
		newVersion.Format(tmpStr, versionStr);
	} else {
		newVersion.LoadString(IDS_ASPI_NOT_LOADED);
	}
	pStatic->SetWindowText(newVersion);

	//ShowRegistrationInfo();
	{
			CWnd* pWnd = GetDlgItem(IDC_ABOUT_ENTER_REG);
			if (pWnd != nil) {
				pWnd->EnableWindow(FALSE);
				pWnd->ShowWindow(FALSE);
			}
	}

	return CDialog::OnInitDialog();
}

#if 0
/*
 * Set the appropriate fields in the dialog box.
 *
 * This is called during initialization and after new registration data is
 * entered successfully.
 */
void
AboutDialog::ShowRegistrationInfo(void)
{
	/*
	 * Pull out the registration info.  We shouldn't need to do much in the
	 * way of validation, since it should have been validated either before
	 * the program finished initializing or before we wrote the values into
	 * the registry.  It's always possible that somebody went and messed with
	 * the registry while we were running -- perhaps a different instance of
	 * CiderPress -- but that should be rare enough that we don't have to
	 * worry about the occasional ugliness.
	 */
	const int kDay = 24 * 60 * 60;
	CString user, company, reg, versions, expire;
	CWnd* pUserWnd;
	CWnd* pCompanyWnd;
	//CWnd* pExpireWnd;

	pUserWnd = GetDlgItem(IDC_REG_USER_NAME);
	ASSERT(pUserWnd != nil);
	pCompanyWnd = GetDlgItem(IDC_REG_COMPANY_NAME);
	ASSERT(pCompanyWnd != nil);
	//pExpireWnd = GetDlgItem(IDC_REG_EXPIRES);
	//ASSERT(pExpireWnd != nil);

	if (gMyApp.fRegistry.GetRegistration(&user, &company, &reg, &versions,
			&expire) == 0)
	{
		if (reg.IsEmpty()) {
			/* not registered, show blank stuff */
			CString unreg;
			unreg.LoadString(IDS_ABOUT_UNREGISTERED);
			pUserWnd->SetWindowText(unreg);
			pCompanyWnd->SetWindowText("");

			/* show expire date */
			time_t expireWhen;
			expireWhen = atol(expire);
			if (expireWhen > 0) {
				CString expireStr;
				time_t now = time(nil);
				expireStr.Format(IDS_REG_EVAL_REM,
					((expireWhen - now) + kDay-1) / kDay);
				/* leave pUserWnd and pCompanyWnd set to defaults */
				pCompanyWnd->SetWindowText(expireStr);
			} else {
				pCompanyWnd->SetWindowText(_T("Has already expired!"));
			}
		} else {
			/* show registration info */
			pUserWnd->SetWindowText(user);
			pCompanyWnd->SetWindowText(company);
			//pExpireWnd->SetWindowText("");

			/* remove "Enter Registration" button */
			CWnd* pWnd = GetDlgItem(IDC_ABOUT_ENTER_REG);
			if (pWnd != nil) {
				pWnd->EnableWindow(FALSE);
			}
		}
	}
}
#endif


/*
 * User hit the "Credits" button.
 */
void
AboutDialog::OnAboutCredits(void)
{
	WinHelp(HELP_TOPIC_CREDITS, HELP_CONTEXT /*HELP_CONTEXTPOPUP*/);
}

#if 0
/*
 * User hit "enter registration" button.  Bring up the appropriate dialog.
 */
void
AboutDialog::OnEnterReg(void)
{
	if (EnterRegDialog::GetRegInfo(this) == 0) {
		ShowRegistrationInfo();
	}
}
#endif
