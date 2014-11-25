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
 *      PIDL functions
 * ==========================================================================
 */

LPITEMIDLIST Pidl::Next(LPCITEMIDLIST pidl)
{
   LPSTR lpMem = (LPSTR)pidl;

   lpMem += pidl->mkid.cb;

   return (LPITEMIDLIST)lpMem;
}

UINT Pidl::GetSize(LPCITEMIDLIST pidl)
{
    UINT cbTotal = 0;

    if (pidl) {
        cbTotal += sizeof(pidl->mkid.cb);       // Null terminator
        while (pidl->mkid.cb) {
            cbTotal += pidl->mkid.cb;
            pidl = Next(pidl);
        }
    }

    return cbTotal;
}

LPITEMIDLIST Pidl::CreatePidl(UINT cbSize)
{
    LPMALLOC lpMalloc;
    HRESULT hr;
    LPITEMIDLIST pidl = NULL;

    hr=SHGetMalloc(&lpMalloc);

    if (FAILED(hr))
       return 0;

    pidl = (LPITEMIDLIST)lpMalloc->Alloc(cbSize);

    if (pidl)
        memset(pidl, 0, cbSize);      // zero-init for external task   alloc

    if (lpMalloc) lpMalloc->Release();

    return pidl;
}

LPITEMIDLIST Pidl::ConcatPidls(LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2)
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

LPITEMIDLIST Pidl::CopyITEMID(LPMALLOC lpMalloc, LPITEMIDLIST lpi)
{
    LPITEMIDLIST lpiTemp;
    
    lpiTemp = (LPITEMIDLIST)lpMalloc->Alloc(lpi->mkid.cb + sizeof(lpi->mkid.cb));
    CopyMemory((PVOID)lpiTemp, (CONST VOID *)lpi,
        lpi->mkid.cb + sizeof(lpi->mkid.cb));
    
    return lpiTemp;
}

BOOL Pidl::GetName(LPSHELLFOLDER lpsf, LPITEMIDLIST lpi, DWORD dwFlags,
    CString* pFriendlyName)
{
    BOOL   bSuccess=TRUE;
    STRRET str;

    if (NOERROR == lpsf->GetDisplayNameOf(lpi, dwFlags, &str)) {
        switch (str.uType) {
        case STRRET_WSTR:
            //WideCharToMultiByte(CP_ACP,             // CodePage
            //                    0,                  // dwFlags
            //                    str.pOleStr,        // lpWideCharStr
            //                    -1,                 // cchWideChar
            //                    lpFriendlyName,     // lpMultiByteStr
            //                    MAX_PATH,           // cchMultiByte
            //                    NULL,               // lpDefaultChar,
            //                    NULL);              // lpUsedDefaultChar

            ////Once the the function returns, the wide string
            ////should be freed. CoTaskMemFree(str.pOleStr) seems
            ////to do the job as well.
            //LPMALLOC pMalloc;
            //SHGetMalloc(&pMalloc); 
            //pMalloc->Free (str.pOleStr);
            //pMalloc->Release();
            *pFriendlyName = str.pOleStr;
            CoTaskMemFree(str.pOleStr);
            break;

         case STRRET_OFFSET:
             *pFriendlyName = (LPSTR)lpi+str.uOffset;
             break;

         case STRRET_CSTR:
             *pFriendlyName = (LPSTR)str.cStr;
             break;

         default:
             bSuccess = FALSE;
             break;
        }
    } else {
        bSuccess = FALSE;
    }

    return bSuccess;
}

LPITEMIDLIST Pidl::GetFullyQualPidl(LPSHELLFOLDER lpsf, LPITEMIDLIST lpi)
{
    //char szBuff[MAX_PATH];
    //OLECHAR szOleChar[MAX_PATH];
    CString name;
    WCHAR pathBuf[MAX_PATH];
    LPSHELLFOLDER lpsfDeskTop;
    LPITEMIDLIST lpifq;
    ULONG ulAttribs = 0;
    HRESULT hr;
    
    if (!GetName(lpsf, lpi, SHGDN_FORPARSING, &name))
        return NULL;
    
    hr = SHGetDesktopFolder(&lpsfDeskTop);
    
    if (FAILED(hr))
        return NULL;
    
    //MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szBuff, -1,
    //                   (USHORT *)szOleChar, sizeof(szOleChar));
    wcscpy_s(pathBuf, name);
    
    hr = lpsfDeskTop->ParseDisplayName(NULL, NULL, pathBuf,
            NULL, &lpifq, &ulAttribs);
    
    lpsfDeskTop->Release();
    
    if (FAILED(hr))
        return NULL;
    
    return lpifq;
}

BOOL Pidl::GetPath(LPCITEMIDLIST pidl, CString* pPath)
{
    BOOL result;
    WCHAR buf[MAX_PATH];

    result = SHGetPathFromIDList(pidl, buf);
    if (result)
        *pPath = buf;

    return result;
}



#if 0
/****************************************************************************
*
*  FUNCTION: DoTheMenuThing(HWND hwnd, 
*                           LPSHELLFOLDER lpsfParent,
*                           LPITEMIDLIST  lpi,
*                           LPPOINT lppt)
*
*  PURPOSE: Displays a popup context menu, given a parent shell folder,
*           relative item id and screen location.
*
*  PARAMETERS:
*    hwnd       - Parent window handle
*    lpsfParent - Pointer to parent shell folder.
*    lpi        - Pointer to item id that is relative to lpsfParent
*    lppt       - Screen location of where to popup the menu.
*
*  RETURN VALUE:
*    Returns TRUE on success, FALSE on failure
*
****************************************************************************/
BOOL
Pidl::DoTheMenuThing(HWND hwnd, LPSHELLFOLDER lpsfParent,
     LPITEMIDLIST  lpi, LPPOINT lppt)
{
    LPCONTEXTMENU lpcm;
    HRESULT       hr;
    char          szTemp[64];
    CMINVOKECOMMANDINFO cmi;
//    DWORD               dwAttribs=0;
    int                 idCmd;
    HMENU               hMenu;
    BOOL                bSuccess=TRUE;

    hr=lpsfParent->GetUIObjectOf(hwnd,
        1,  //Number of objects to get attributes of
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
                cmi.nShow        = SW_SHOWNORMAL;
                cmi.dwHotKey     = 0;
                cmi.hIcon        = NULL;
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

int Pidl::GetItemIcon(LPITEMIDLIST lpi, UINT uFlags)
{
    SHFILEINFO sfi = { 0 };
    
    uFlags |= SHGFI_PIDL;   // we're passing a PIDL, not a pathname, in 1st arg
    SHGetFileInfo((LPCWSTR)lpi, 0, &sfi, sizeof(SHFILEINFO), uFlags);
    
    return sfi.iIcon;
}
