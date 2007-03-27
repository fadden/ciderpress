/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * PIDL utility functions.
 */
#include "StdAfx.h"
#include "Pidl.h"

/*
 * ==========================================================================
 *		PIDL functions
 * ==========================================================================
 */

/*
 * Return the next item in the PIDL.
 *
 * "pidl->mkid.cb" will be zero at the end of the list.
 */
LPITEMIDLIST
Pidl::Next(LPCITEMIDLIST pidl)
{
   LPSTR lpMem = (LPSTR)pidl;

   lpMem += pidl->mkid.cb;

   return (LPITEMIDLIST)lpMem;
}

/*
 * Compute the size in bytes of a PIDL.
 */
UINT
Pidl::GetSize(LPCITEMIDLIST pidl)
{
	UINT cbTotal = 0;

	if (pidl) {
		cbTotal += sizeof(pidl->mkid.cb);		// Null terminator
		while (pidl->mkid.cb) {
			cbTotal += pidl->mkid.cb;
			pidl = Next(pidl);
		}
	}

	return cbTotal;
}

/*
 * Allocate a PIDL of the specified size.
 */
LPITEMIDLIST
Pidl::CreatePidl(UINT cbSize)
{
	LPMALLOC lpMalloc;
	HRESULT hr;
	LPITEMIDLIST pidl = NULL;

	hr=SHGetMalloc(&lpMalloc);

	if (FAILED(hr))
	   return 0;

	pidl = (LPITEMIDLIST)lpMalloc->Alloc(cbSize);

	if (pidl)
		memset(pidl, 0, cbSize);	  // zero-init for external task   alloc

	if (lpMalloc) lpMalloc->Release();

	return pidl;
}

/*
 * Concatenates two PIDLs.  The PIDL returned is newly-allocated storage.
 *
 * "pidl1" may be NULL.
 */
LPITEMIDLIST
Pidl::ConcatPidls(LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2)
{
	LPITEMIDLIST pidlNew;
	UINT cb1;
	UINT cb2;

	if (pidl1)
	   cb1 = GetSize(pidl1) - sizeof(pidl1->mkid.cb);
	else
	   cb1 = 0;

	cb2 = GetSize(pidl2);

	pidlNew = CreatePidl(cb1 + cb2);
	if (pidlNew) {
		if (pidl1)
		   memcpy(pidlNew, pidl1, cb1);
		memcpy(((LPSTR)pidlNew) + cb1, pidl2, cb2);
	}
	return pidlNew;
}

/*
 * Make a copy of a PIDL.
 *
 * The PIDL returned is newly-allocated storage.
 */
LPITEMIDLIST
Pidl::CopyITEMID(LPMALLOC lpMalloc, LPITEMIDLIST lpi)
{
	LPITEMIDLIST lpiTemp;
	
	lpiTemp = (LPITEMIDLIST)lpMalloc->Alloc(lpi->mkid.cb + sizeof(lpi->mkid.cb));
	CopyMemory((PVOID)lpiTemp, (CONST VOID *)lpi,
		lpi->mkid.cb + sizeof(lpi->mkid.cb));
	
	return lpiTemp;
}


/*
 * Get the display name of a file in a ShellFolder.
 *
 * "lpsf" is the ShellFolder that contains the file, "lpi" is the PIDL for
 * the file, "dwFlags" is passed to GetDisplayNameOf and affects which
 * name is returned, and "lpFriendlyName" is a buffer of at least MAX_PATH
 * bytes.
 */
BOOL
Pidl::GetName(LPSHELLFOLDER lpsf, LPITEMIDLIST lpi, DWORD dwFlags,
	LPSTR lpFriendlyName)
{
	BOOL   bSuccess=TRUE;
	STRRET str;

	if (NOERROR == lpsf->GetDisplayNameOf(lpi, dwFlags, &str))
	{
		switch (str.uType) {
		case STRRET_WSTR:
			WideCharToMultiByte(CP_ACP,				// CodePage
								0,					// dwFlags
								str.pOleStr,		// lpWideCharStr
								-1,					// cchWideChar
								lpFriendlyName,		// lpMultiByteStr
								MAX_PATH,			// cchMultiByte
								NULL,				// lpDefaultChar,
								NULL);				// lpUsedDefaultChar

			//Once the the function returns, the wide string
			//should be freed. CoTaskMemFree(str.pOleStr) seems
			//to do the job as well.
			LPMALLOC pMalloc;
			SHGetMalloc(&pMalloc); 
			pMalloc->Free (str.pOleStr);
			pMalloc->Release();
			break;

		 case STRRET_OFFSET:
			 lstrcpy(lpFriendlyName, (LPSTR)lpi+str.uOffset);
			 break;

		 case STRRET_CSTR:
			 lstrcpy(lpFriendlyName, (LPSTR)str.cStr);
			 break;

		 default:
			 bSuccess = FALSE;
			 break;
		}
	}
	else
	  bSuccess = FALSE;

	return bSuccess;
}



/*
 * Get a fully qualified PIDL for a ShellFolder.
 *
 * This is a rather roundabout way of doing things (converting to a full
 * display name and then converting that to a PIDL).  However, there doesn't
 * seem to be a way to just ask a ShellFolder for its fully qualified PIDL.
 *
 * Pass in the parent ShellFolder and the item's partial PIDL.
 */
LPITEMIDLIST
Pidl::GetFullyQualPidl(LPSHELLFOLDER lpsf, LPITEMIDLIST lpi)
{
	char szBuff[MAX_PATH];
	OLECHAR szOleChar[MAX_PATH];
	LPSHELLFOLDER lpsfDeskTop;
	LPITEMIDLIST lpifq;
	ULONG ulEaten, ulAttribs;
	HRESULT hr;
	
	if (!GetName(lpsf, lpi, SHGDN_FORPARSING, szBuff))
		return NULL;
	
	hr = SHGetDesktopFolder(&lpsfDeskTop);
	
	if (FAILED(hr))
		return NULL;
	
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szBuff, -1,
					   (USHORT *)szOleChar, sizeof(szOleChar));
	
	hr = lpsfDeskTop->ParseDisplayName(NULL, NULL, szOleChar,
			&ulEaten, &lpifq, &ulAttribs);
	
	lpsfDeskTop->Release();
	
	if (FAILED(hr))
		return NULL;
	
	return lpifq;
}

/*
 * Convert a PIDL to a filesystem path.
 *
 * Returns TRUE on success, FALSE on failure.
 */
BOOL
Pidl::GetPath(LPCITEMIDLIST pidl, CString* pPath)
{
	BOOL result;
	char buf[MAX_PATH];

	result = SHGetPathFromIDList(pidl, buf);
	if (result)
		*pPath = buf;

	return result;
}



#if 0
/****************************************************************************
*
*  FUNCTION: DoTheMenuThing(HWND hwnd, 
*							LPSHELLFOLDER lpsfParent,
*							LPITEMIDLIST  lpi,
*							LPPOINT lppt)
*
*  PURPOSE: Displays a popup context menu, given a parent shell folder,
*			relative item id and screen location.
*
*  PARAMETERS:
*	 hwnd		- Parent window handle
*	 lpsfParent - Pointer to parent shell folder.
*	 lpi		- Pointer to item id that is relative to lpsfParent
*	 lppt		- Screen location of where to popup the menu.
*
*  RETURN VALUE:
*	 Returns TRUE on success, FALSE on failure
*
****************************************************************************/
BOOL
Pidl::DoTheMenuThing(HWND hwnd, LPSHELLFOLDER lpsfParent,
	 LPITEMIDLIST  lpi, LPPOINT lppt)
{
	LPCONTEXTMENU lpcm;
	HRESULT 	  hr;
	char		  szTemp[64];
	CMINVOKECOMMANDINFO cmi;
//	  DWORD 			  dwAttribs=0;
	int 				idCmd;
	HMENU				hMenu;
	BOOL				bSuccess=TRUE;

	hr=lpsfParent->GetUIObjectOf(hwnd,
		1,	//Number of objects to get attributes of
		(const struct _ITEMIDLIST **)&lpi,
		IID_IContextMenu,
		0,
		(LPVOID *)&lpcm);
	if (SUCCEEDED(hr))	
	{
	   hMenu = CreatePopupMenu();

	   if (hMenu)
	   {
		  hr=lpcm->QueryContextMenu(hMenu, 0, 1, 0x7fff, CMF_EXPLORE);
		  if (SUCCEEDED(hr))
		  {
			 idCmd=TrackPopupMenu(hMenu, 
				TPM_LEFTALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON, 
				lppt->x, lppt->y, 0, hwnd, NULL);

			 if (idCmd)
			 {
				cmi.cbSize = sizeof(CMINVOKECOMMANDINFO);
				cmi.fMask  = 0;
				cmi.hwnd   = hwnd;
				cmi.lpVerb = MAKEINTRESOURCE(idCmd-1);
				cmi.lpParameters = NULL;
				cmi.lpDirectory  = NULL;
				cmi.nShow		 = SW_SHOWNORMAL;
				cmi.dwHotKey	 = 0;
				cmi.hIcon		 = NULL;
				hr=lpcm->InvokeCommand(&cmi);
				if (!SUCCEEDED(hr))  
				{
				   wsprintf(szTemp, "InvokeCommand failed. hr=%lx", hr);
				   AfxMessageBox(szTemp);
				}
			 }

		  }
		  else
			 bSuccess = FALSE;

		  DestroyMenu(hMenu);
	   }
	   else
		  bSuccess = FALSE;

	   lpcm->Release();
	} 
	else
	{
	   wsprintf(szTemp, "GetUIObjectOf failed! hr=%lx", hr);
	   AfxMessageBox(szTemp );
	   bSuccess = FALSE;
	}
	return bSuccess;
}
#endif

/*
 * Get the index for an icon for a ShellFolder object.
 *
 * "lpi" is the fully-qualified PIDL for the object in question.  "uFlags"
 * specifies which of the object's icons to retrieve.
 */
int
Pidl::GetItemIcon(LPITEMIDLIST lpi, UINT uFlags)
{
	SHFILEINFO sfi;
	
	uFlags |= SHGFI_PIDL;
	SHGetFileInfo((LPCSTR)lpi, 0, &sfi, sizeof(SHFILEINFO), uFlags);
	
	return sfi.iIcon;
}
