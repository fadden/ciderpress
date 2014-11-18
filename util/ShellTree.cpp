/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * ShellTree, a TreeCtrl derivative for displaying the Windows shell namespace.
 */
#include "StdAfx.h"
#include "ShellTree.h"
#include "Pidl.h"
#include "PathName.h"


/*
 * ==========================================================================
 *      ShellTree
 * ==========================================================================
 */

BEGIN_MESSAGE_MAP(ShellTree, CTreeCtrl)
    ON_NOTIFY_REFLECT(TVN_ITEMEXPANDING, OnFolderExpanding)
    ON_NOTIFY_REFLECT(TVN_DELETEITEM, OnDeleteShellItem)
    ON_NOTIFY_REFLECT_EX(TVN_SELCHANGED, OnSelectionChange)
END_MESSAGE_MAP()


/*
 * Replace a CTreeCtrl in a dialog box with us.  All of the styles are
 * copied from the original dialog window.
 *
 * Returns TRUE on success, FALSE on failure.
 */
BOOL
ShellTree::ReplaceDlgCtrl(CDialog* pDialog, int treeID)
{
    CWnd* pWnd = pDialog->GetDlgItem(treeID);
    if (pWnd == NULL)
        return FALSE;

#if 0
    DWORD styles = pWnd->GetStyle();
    DWORD stylesEx = pWnd->GetExStyle();
    CRect rect;
    pWnd->GetWindowRect(&rect);
    pDialog->ScreenToClient(&rect);

    pWnd->DestroyWindow();
    CreateEx(stylesEx, WC_TREEVIEW, NULL, styles, rect, pDialog, treeID);
#endif

    /* latch on to their window handle */
    Attach(pWnd->m_hWnd);

    return TRUE;
}

/*
 * Populate the tree, starting from "nFolder".
 *
 * Returns TRUE on success, FALSE on failure.
 */
BOOL
ShellTree::PopulateTree(int nFolder) 
{
    LPSHELLFOLDER lpsf = NULL, lpsf2 = NULL;
    LPITEMIDLIST lpi = NULL;
    TV_SORTCB tvscb;
    LPMALLOC lpMalloc = NULL;
    HRESULT hr;
    BOOL retval = FALSE;

    // Grab a malloc handle.
    hr = ::SHGetMalloc(&lpMalloc);
    if (FAILED(hr))
        return FALSE;

    // Get a pointer to the desktop folder.
    hr = SHGetDesktopFolder(&lpsf);
    if (FAILED(hr))
        goto bail;

    // Initialize the tree view to be empty.
    DeleteAllItems();

    if (nFolder == CSIDL_DESKTOP) {
        // already done
        lpsf2 = lpsf;
        lpsf = NULL;
        ASSERT(lpi == NULL);
    } else {
        // find the desired special folder
        hr = SHGetSpecialFolderLocation(m_hWnd, nFolder, &lpi);
        if (FAILED(hr)) {
            LOGI("BUG: could not find requested special folder");
            goto bail;
        }

        // bind a ShellFolder to the PIDL.
        hr = lpsf->BindToObject(lpi, 0, IID_IShellFolder, (LPVOID *)&lpsf2);
        if (FAILED(hr))
            goto bail;
    }

    // fill in the tree starting from this point
    FillTreeView(lpsf2, lpi, TVI_ROOT);

    // Sort the items in the tree view
    tvscb.hParent     = TVI_ROOT;
    tvscb.lParam      = 0;
    tvscb.lpfnCompare = TreeViewCompareProc;
    SortChildrenCB(&tvscb);

bail:
    if (lpsf != NULL)
        lpsf->Release();
    if (lpsf != NULL)
        lpsf2->Release();
    lpMalloc->Free(lpi);

    return retval;
}

/*
 * Open up and select My Computer.
 */
void
ShellTree::ExpandMyComputer(void)
{
    HTREEITEM hItem;
    hItem = FindMyComputer();
    if (hItem == NULL)
        hItem = GetRootItem();
    Expand(hItem, TVE_EXPAND);
    Select(hItem, TVGN_CARET);
}


/*
 * Fills a branch of the TreeView control.  Given the shell folder (both as
 * a shell folder and the fully-qualified item ID list to it) and the parent
 * item in the tree (TVI_ROOT to start off), add all the kids to the tree.
 *
 * Does not try to add the current entry, as a result of which we don't
 * have a root "Desktop" node that everything is a child of.  This is okay.
 */
void
ShellTree::FillTreeView(LPSHELLFOLDER lpsf, LPITEMIDLIST lpifq,
    HTREEITEM hParent)
{
    CWaitCursor     wait;
    HTREEITEM       hPrev = NULL;                 // Previous Item Added.
    LPENUMIDLIST    lpe=NULL;
    LPITEMIDLIST    lpi=NULL;
    LPMALLOC        lpMalloc=NULL;
    ULONG           ulFetched;
    HRESULT         hr;
    HWND            hwnd=::GetParent(m_hWnd);
    bool            gotOne = false;

    // Allocate a shell memory object. 
    hr = ::SHGetMalloc(&lpMalloc);
    if (FAILED(hr))
       return;

    // Get the IEnumIDList object for the given folder.
    hr = lpsf->EnumObjects(hwnd,
            SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN,
            &lpe);

    if (SUCCEEDED(hr))
    {
        // Enumerate throught the list of folder and non-folder objects.
        while (S_OK == lpe->Next(1, &lpi, &ulFetched))
        {
            //Create a fully qualified path to the current item
            //The SH* shell api's take a fully qualified path pidl,
            //(see GetIcon above where I call SHGetFileInfo) whereas the
            //interface methods take a relative path pidl.
            ULONG ulAttrs = SFGAO_HASSUBFOLDER | SFGAO_FOLDER |
                SFGAO_FILESYSANCESTOR | SFGAO_DROPTARGET | SFGAO_HIDDEN;
            bool goodOne;

            // Determine what type of object we have.
            lpsf->GetAttributesOf(1, (const struct _ITEMIDLIST **)&lpi, &ulAttrs);

#if 1
            {   /* DEBUG */
                CString name;
                if (Pidl::GetName(lpsf, lpi, SHGDN_NORMAL, &name)) {
                    LOGI(" Checking '%ls' 0x%08lx",
                        name, ulAttrs);
                } else {
                    LOGI(" Checking <no-name> 0x%08lx",
                        ulAttrs);
                }
            }
#endif

            /*
             * (This should be converted to a table and automatically
             * scanned when assertions are enabled.)
             *
             * Win2K folders to show:
             *  'My Computer'           0xb0000100 [at root]
             *  'My Network Places'     0xb0000100 [at root]
             *  'My Documents'          0xb0000100 [at root]
             *  'WIN (C:)'              0xb0000100
             *  'Compact Disc (H:)'     0xb0000100  <-- SFGAO_REMOVABLE not set
             *  'Removable Disk (L:)'   0xb0000100
             *  'Documents and Settings' 0xf0400177
             *  'Entire Network'        0xb0000000
             *  'Microsoft Windows Network' 0xb0000000
             *  'Computers Near Me'     0xa0000100
             *  'ftp.apple.asimov.net'  0xa0000100
             *  'QA-C-Recv on QA'       0xa0000100
             *  'BACKUP'                0xf0400177
             *  'dudley'                0x70400177
             *
             * Win2K files, folders, etc. to hide:
             *  'gVim 6.1'              0x00000100
             *  'Internet Explorer'     0x20000000
             *  'Recycle Bin'           0x20000100 (folder + droptarget)
             *  'Control Panel'         0xa0000000
             *  'Scheduled Tasks on Shiny'  0x20000000
             *  'Add Network Place'     0x00000000
             *  'ColorBars.jpg'         0x40400177
             *
             * Win98 folders to show:
             *  'My Computer'           0xb0000100 [at root]
             *  'My Documents'          0xb0000100 [at root]
             *  'Network Neighborhood'  0xb0000100 [at root]
             *  'WIN98 (C:)'            0xb0000100
             *  [C:\]'My Documents'     0xf8000177
             *  'Entire Network'        0xb0000000
             *  'cpt'                   0xe0000177
             *  'My eBooks'             0x60000177
             *
             * Win98 folders and stuff to hide:
             *  'Control Panel'         0x20000000
             *  'Printers'              0x20000100 (folder + droptarget)
             *  'Web Folders'           0xa0000000
             *  'BOOTLOG.TXT'           0x40080177
             *
             * Note that Win98 folders on disk don't have FILESYSANCESTOR
             * set.  If we check FOLDER && FILESYSTEM (0x60000000), we get
             * anything starting with 6/7/E/F, which appears safe.  We
             * need to do additional tests to pick up some of the A/B items
             * that we want while hiding the A items we don't want.
             *
             * FILESYSANCESTOR is 0x10000000, so that plus FOLDER allows
             * 3/7/b/f.  These appear to be entirely okay.
             *
             * DROPTARGET is 0x00000100, and HASSUBFOLDER is 0x80000000.
             * Combining with FOLDER yields 0xa00000100, allowing A/B/E/F.
             * The only at-risk is A, but combined with DROPTARGET we
             * seem to screen out all the bad ones.
             */

            if (lpifq == NULL) {
                /* dealing with stuff at the root level */
                goodOne = ( (ulAttrs & SFGAO_FOLDER) &&
                            (ulAttrs & SFGAO_HASSUBFOLDER) &&
                            (ulAttrs & SFGAO_FILESYSANCESTOR) );
            } else {
                /* deeper down, we're picky in different ways */
                bool isFolder = (ulAttrs & SFGAO_FOLDER) != 0;
                bool fileSys = (ulAttrs & SFGAO_FILESYSTEM) != 0;
                bool hasFSAncestor = (ulAttrs & SFGAO_FILESYSANCESTOR) != 0;
                bool dropAndSub = (ulAttrs & SFGAO_DROPTARGET) != 0 &&
                                  (ulAttrs & SFGAO_HASSUBFOLDER) != 0;

                goodOne = isFolder &&
                            (fileSys || hasFSAncestor || dropAndSub);
            }

            if (goodOne) {
                gotOne = true;

                if (!AddNode(lpsf, lpi, lpifq, ulAttrs, hParent, &hPrev)) {
                    LOGI("AddNode failed!");
                    goto Done;
                }
            }

            lpMalloc->Free(lpi);  //Free the pidl that the shell gave us.
            lpi=0;
        }
    }

    // Sometimes SFGAO_HASSUBFOLDERS lies, notably in Network Neighborhood.
    // When we actually scan the directory we can update the parent node
    // if it turns out there's nothing underneath.
    if (!gotOne) {
        TVITEM tvi;
        CString name = GetItemText(hParent);
        tvi.hItem = hParent;
        tvi.mask = TVIF_CHILDREN;
        if (!GetItem(&tvi)) {
            LOGI("Could not get TV '%ls'", name);
            ASSERT(false);
        } else if (tvi.cChildren) {
            LOGI("Removing child count (%d) from '%ls'",
                tvi.cChildren, name);
            tvi.cChildren = 0;
            if (!SetItem(&tvi)) {
                LOGI("Could not set TV '%ls'", name);
                ASSERT(false);
            }
        }
    }

Done:
    if (lpe)  
        lpe->Release();

    //The following 2 if statements will only be TRUE if we got here on an
    //error condition from the "goto" statement.  Otherwise, we free this memory
    //at the end of the while loop above.
    if (lpi && lpMalloc)           
        lpMalloc->Free(lpi);
    if (lpMalloc) 
        lpMalloc->Release();

    //LOGI("FillTreeView DONE");
}

/*
 * Add a node to the tree.
 *
 * Returns TRUE on success, FALSE on failure.
 */
BOOL
ShellTree::AddNode(LPSHELLFOLDER lpsf, LPITEMIDLIST lpi, LPITEMIDLIST lpifq,
    unsigned long ulAttrs, HTREEITEM hParent, HTREEITEM* phPrev)
{
    TVITEM          tvi;
    TVINSERTSTRUCT  tvins;
    LPITEMIDLIST    lpifqThisItem = NULL;
    TVItemData*     lptvid = NULL;
    WCHAR           szBuff[MAX_PATH];
    CString         name;
    LPMALLOC        lpMalloc = NULL;
    HRESULT         hr;
    BOOL            result = FALSE;

    hr = ::SHGetMalloc(&lpMalloc);
    if (FAILED(hr))
       return FALSE;

    //Now get the friendly name that we'll put in the treeview.
    if (!Pidl::GetName(lpsf, lpi, SHGDN_NORMAL, &name)) {
        LOGI("HEY: failed getting friendly name");
        goto bail; // Error - could not get friendly name.
    }
    wcscpy_s(szBuff, name);
    //LOGI("AddNode '%ls' ATTR=0x%08lx", szBuff, ulAttrs);

    lptvid = (TVItemData*)lpMalloc->Alloc(sizeof(TVItemData));
    if (!lptvid)
        goto bail;

    tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE |
                TVIF_PARAM;

    if (ulAttrs & SFGAO_HASSUBFOLDER) {
        //This item has sub-folders, so let's put the + in the TreeView.
        //The first time the user clicks on the item, we'll populate the
        //sub-folders.
        tvi.mask |= TVIF_CHILDREN;
        tvi.cChildren = 1;
    }

    tvi.pszText  = szBuff;
    tvi.cchTextMax = MAX_PATH;      // (not needed for InsertItem)

    // Allocate a fully-qualified PIDL and stuff it in.
    lpifqThisItem = Pidl::ConcatPidls(lpifq, lpi);

    // Add the icons.
    GetNormalAndSelectedIcons(lpifqThisItem, &tvi);

    // Done with lipfqThisItem.
    lptvid->lpifq = lpifqThisItem;
    lpifqThisItem = NULL;

    // Put in a copy of the relative PIDL.
    lptvid->lpi = Pidl::CopyITEMID(lpMalloc, lpi);

    // Stuff the parent folder's lpsf in.
    lptvid->lpsfParent = lpsf;
    lpsf->AddRef();

    // Done with lptvid.
    tvi.lParam = (LPARAM)lptvid;
    lptvid = NULL;

    // Populate the TreeView Insert Struct
    // The item is the one filled above.
    // Insert it after the last item inserted at this level.
    // And indicate this is a root entry.
    tvins.item       = tvi;
    tvins.hInsertAfter = *phPrev;
    tvins.hParent    = hParent;

    // Add the item to the tree
    *phPrev = InsertItem(&tvins);

    result = TRUE;

bail:
    lpMalloc->Release();
    return result;
}

/*
 * Set the TreeView normal and selected icons for the specified entry.
 *
 * "lpifq" is the fully-qualified PIDL, LPTV_ITEM is an item in the tree.
 */
void
ShellTree::GetNormalAndSelectedIcons(LPITEMIDLIST lpifq,
                               LPTV_ITEM lptvitem)
{
   //Note that we don't check the return value here because if GetIcon()
   //fails, then we're in big trouble...

    lptvitem->iImage = Pidl::GetItemIcon(lpifq,
                              SHGFI_SYSICONINDEX | 
                              SHGFI_SMALLICON);
   
    lptvitem->iSelectedImage = Pidl::GetItemIcon(lpifq,
                                      SHGFI_SYSICONINDEX | 
                                      SHGFI_SMALLICON |
                                      SHGFI_OPENICON);
   
   return;
}



/*
 * Sort function callback for TreeView SortChildrenCB.
 */
int CALLBACK 
ShellTree::TreeViewCompareProc(LPARAM lparam1, LPARAM lparam2, LPARAM)
{
    TVItemData* lptvid1 = (TVItemData*)lparam1;
    TVItemData* lptvid2 = (TVItemData*)lparam2;
    HRESULT hr;

    hr = lptvid1->lpsfParent->CompareIDs(0, lptvid1->lpi, lptvid2->lpi);
    
    if (FAILED(hr)) {
        ASSERT(false);
        return 0;
    }

#if 0
    if (lptvid1->alphaSort && lptvid2->alphaSort) {
        char buf1[MAX_PATH], buf2[MAX_PATH];
        if (Pidl::GetName(lptvid1->lpsfParent, lptvid1->lpi, SHGDN_NORMAL, buf1) &&
            Pidl::GetName(lptvid2->lpsfParent, lptvid2->lpi, SHGDN_NORMAL, buf2))
        {
            LOGI("COMPARING '%s' to '%s' (res=%d)", buf1, buf2,
                (short) HRESULT_CODE(hr));
            return stricmp(buf1, buf2);
        } else {
            ASSERT(false);
            return 0;
        }
    }
#endif

    return (short) HRESULT_CODE(hr);
}


/*
 * Add a new folder to the tree at the currently-selected node.  This may
 * not actually add a folder if the new folder is at a point in the tree
 * below where we have already expanded.
 *
 * Returns TRUE on success, or FALSE on failure.
 */
BOOL
ShellTree::AddFolderAtSelection(const CString& name)
{
    LPSHELLFOLDER lpsf = NULL;
    LPITEMIDLIST lpi = NULL;
    HTREEITEM hParent;
    LPMALLOC lpMalloc = NULL;
    LPENUMIDLIST lpe = NULL;
    const TVItemData* parentTvid;
    TVItemData* newTvid = NULL;
    HWND hwnd = ::GetParent(m_hWnd);
    HTREEITEM hPrev = NULL;
    BOOL result = false;
    CString debugName;
    HRESULT hr;

    LOGI("AddFolderAtSelection '%ls'", name);

    // Allocate a shell memory object. 
    hr = ::SHGetMalloc(&lpMalloc);
    if (FAILED(hr))
       return FALSE;

    hParent = GetSelectedItem();
    if (hParent == NULL) {
        LOGI("Nothing selected!");
        goto bail;
    }

    /*
     * Now we either need to create a new node in an existing tree, or if
     * we haven't expanded the current node yet then we can just let the
     * usual FillTree mechanism do it for us.
     *
     * If the current node is marked as having children, but has no
     * child structures, then it's a folder with sub-folders that hasn't
     * been filled in yet.  We don't need to do anything.
     *
     * If the current node doesn't have children, then this is a leaf
     * node that has just become a branch.  We update its "#of kids" state
     * and again let the usual mechanisms do their work.
     *
     * If the current node has expanded children, then we need to do the
     * work ourselves.  (It's that, or invalidate the entire subtree,
     * which has some UI consequences.)
     */
    TVITEM tvi;
    debugName = GetItemText(hParent);
    tvi.hItem = hParent;
    tvi.mask = TVIF_CHILDREN;
    if (!GetItem(&tvi)) {
        LOGI("Could not get TV '%ls'", debugName);
        ASSERT(false);
    } else {
        HTREEITEM child = GetChildItem(hParent);
        if (child == NULL && tvi.cChildren) {
            LOGI(" Found unexpanded node, not adding %ls", name);
            result = TRUE;
            goto bail;
        } else if (child == NULL && !tvi.cChildren) {
            LOGI(" Found former leaf node, updating kids in %ls", debugName);
            tvi.cChildren = 1;
            if (!SetItem(&tvi)) {
                LOGI("Could not set TV '%ls'", debugName);
                ASSERT(false);
            }
            result = TRUE;
            goto bail;
        } else {
            ASSERT(child != NULL && tvi.cChildren != 0);
            LOGI(" Found expanded branch node '%ls', adding new '%ls'",
                debugName, name);
        }
    }


    parentTvid = (TVItemData*)GetItemData(hParent);
    ASSERT(parentTvid != NULL);

    // Get a handle to the ShellFolder for the currently selected node.
    hr = parentTvid->lpsfParent->BindToObject(parentTvid->lpi,
                0, IID_IShellFolder, (LPVOID *)&lpsf);
    if (FAILED(hr)) {
        LOGI("Glitch: unable to get ShellFolder for selected folder");
        goto bail;
    }

    // Get an enumerator for the selected node.
    hr = lpsf->EnumObjects(hwnd, SHCONTF_FOLDERS | SHCONTF_INCLUDEHIDDEN,
            &lpe);
    if (FAILED(hr)) {
        LOGI("Glitch: unable to get enumerator for selected folder");
        goto bail;
    }

    // Enumerate throught the list of folder and non-folder objects.
    while (S_OK == lpe->Next(1, &lpi, NULL)) {
        CString pidlName;
        if (Pidl::GetName(lpsf, lpi, SHGDN_NORMAL, &pidlName)) {
            if (name.CompareNoCase(pidlName) == 0) {
                /* match! */
                if (!AddNode(lpsf, lpi, parentTvid->lpifq, 0, hParent, &hPrev)) {
                    LOGI("AddNode failed!");
                    goto bail;
                }
                result = TRUE;
                break;
            }
        }

        lpMalloc->Free(lpi);  //Free the pidl that the shell gave us.
        lpi = NULL;
    }

bail:
    if (lpi != NULL)
        lpMalloc->Free(lpi);
    if (lpsf != NULL)
        lpsf->Release();
    if (lpe != NULL)  
        lpe->Release();
    lpMalloc->Release();
    return result;
}


/*
 * Respond to TVN_ITEMEXPANDING message.
 *
 * If the subtree hasn't been expanded yet, dig in.
 */
void ShellTree::OnFolderExpanding(NMHDR* pNMHDR, LRESULT* pResult) 
{
    TVItemData*     lptvid; //Long pointer to TreeView item data
    HRESULT         hr;
    LPSHELLFOLDER   lpsf2=NULL;
    TV_SORTCB       tvscb;

    NM_TREEVIEW* pnmtv = (NM_TREEVIEW*)pNMHDR;
    if (pnmtv->itemNew.state & TVIS_EXPANDEDONCE) {
        LOGI("Already expanded!");
        return;
    }
        
    lptvid = (TVItemData*)pnmtv->itemNew.lParam;
    if (lptvid) {
        hr = lptvid->lpsfParent->BindToObject(lptvid->lpi,
                0, IID_IShellFolder,(LPVOID *)&lpsf2);

        if (SUCCEEDED(hr))
        {
            FillTreeView(lpsf2,
                   lptvid->lpifq,
                   pnmtv->itemNew.hItem);
        }

        tvscb.hParent     = pnmtv->itemNew.hItem;
        tvscb.lParam      = 0;
        tvscb.lpfnCompare = TreeViewCompareProc;

        SortChildrenCB(&tvscb);
    }   
    
    *pResult = 0;
}

#if 0
/****************************************************************************
*
*   FUNCTION:   GetContextMenu(NMHDR* pNMHDR, LRESULT* pResult) 
*
*   PURPOSE:    Diplays a popup menu for the folder selected. Pass the
*               parameters from Rclick() to this function.
*
*   MESSAGEMAP: NM_RCLICK;
*
****************************************************************************/
void ShellTree::GetContextMenu(NMHDR*, LRESULT* pResult) 
{
    POINT           pt;
    TVItemData*     lptvid;  //Long pointer to TreeView item data
    TV_HITTESTINFO  tvhti;
    TV_ITEM         tvi;

    ::GetCursorPos((LPPOINT)&pt);
    ScreenToClient(&pt);
    tvhti.pt=pt;
    HitTest(&tvhti);
    SelectItem(tvhti.hItem);
    if (tvhti.flags & (TVHT_ONITEMLABEL|TVHT_ONITEMICON))
    {
        ClientToScreen(&pt);
        tvi.mask=TVIF_PARAM;
        tvi.hItem=tvhti.hItem;
        
        if (!GetItem(&tvi)){
            return;
        }
        
        lptvid = (TVItemData*)tvi.lParam;
        
        Pidl::DoTheMenuThing(::GetParent(m_hWnd),
            lptvid->lpsfParent, lptvid->lpi, &pt);
    }   
    
    *pResult = 0;
}
#endif

/*
 * Respond to TVN_SELCHANGED notification.
 */
BOOL
ShellTree::OnSelectionChange(NMHDR* pnmh, LRESULT* pResult)
{
    fFolderPathValid = OnFolderSelected(pnmh, pResult, fFolderPath);
    *pResult = 0;
    return FALSE;   // allow window parent to handle notification
}

/*
 * This does the bulk of the work when the selection changes.
 *
 * The filesystem path (if any) to the object is placed in "szFolderPath".
 */
BOOL
ShellTree::OnFolderSelected(NMHDR* pNMHDR, LRESULT* pResult,
    CString &szFolderPath) 
{
    TVItemData*     lptvid;
    LPSHELLFOLDER   lpsf2=NULL;
    WCHAR           szBuff[MAX_PATH];
    HRESULT         hr;
    BOOL            bRet=false;
    HTREEITEM       hItem=NULL;

    hItem = GetSelectedItem();
    if (hItem) {
        lptvid = (TVItemData*)GetItemData(hItem);

        if (lptvid && lptvid->lpsfParent && lptvid->lpi)
        {
            hr = lptvid->lpsfParent->BindToObject(lptvid->lpi,
                     0, IID_IShellFolder, (LPVOID *)&lpsf2);

            if (SUCCEEDED(hr)) {
                ULONG ulAttrs = SFGAO_FILESYSTEM;

                // Determine what type of object we have.
                lptvid->lpsfParent->GetAttributesOf(1,
                    (const struct _ITEMIDLIST **)&lptvid->lpi, &ulAttrs);

                if (ulAttrs & (SFGAO_FILESYSTEM)) {
                    if (SHGetPathFromIDList(lptvid->lpifq, szBuff)){
                        szFolderPath = szBuff;
                        bRet = true;
                    }
                }

                if (bRet) {
                    LOGI("Now selected: '%ls'", szBuff);
                } else {
                    LOGI("Now selected: <no path>");
                }

#if 0
                // If we're expanding into new territory, load the
                // sub-tree.  [This makes it expand things that aren't
                // necessarily going to be opened, which is very bad for
                // empty floppy and CD-ROM drives.  Makes little sense.]
                TV_SORTCB tvscb;
                NM_TREEVIEW* pnmtv = (NM_TREEVIEW*)pNMHDR;
                if ((pnmtv->itemNew.cChildren == 1) &&
                    !(pnmtv->itemNew.state & TVIS_EXPANDEDONCE))
                {
                    FillTreeView(lpsf2, lptvid->lpifq, pnmtv->itemNew.hItem);

                    tvscb.hParent     = pnmtv->itemNew.hItem;
                    tvscb.lParam      = 0;
                    tvscb.lpfnCompare = TreeViewCompareProc;
                    SortChildrenCB(&tvscb);
                    
                    pnmtv->itemNew.state |= TVIS_EXPANDEDONCE;
                    pnmtv->itemNew.stateMask |= TVIS_EXPANDEDONCE;
                    pnmtv->itemNew.mask |= TVIF_STATE;
                    SetItem(&pnmtv->itemNew);
                }
#endif
            }
        }
        if(lpsf2)
            lpsf2->Release();
        
    }   
    *pResult = 0;
    return bRet;
}

/*
 * Handle TVN_DELETEITEM notification by cleaning up our stuff.
 */
void
ShellTree::OnDeleteShellItem(NMHDR* pNMHDR, LRESULT* pResult)
{
    TVItemData* lptvid=NULL;
    HRESULT hr;
    LPMALLOC lpMalloc;

    //LOGI("TVN_DELETEITEM");

    NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;

    //Let's free the memory for the TreeView item data...
    hr = SHGetMalloc(&lpMalloc);
    if (FAILED(hr))
        return;
        
    lptvid = (TVItemData*)pNMTreeView->itemOld.lParam;
    lptvid->lpsfParent->Release();
    lpMalloc->Free(lptvid->lpi);  
    lpMalloc->Free(lptvid->lpifq);  
    lpMalloc->Free(lptvid);  
    lpMalloc->Release();

    *pResult = 0;
}

/*
 * Gets a handle to the system image list (by just grabbing whatever is
 * in place for C:\) and makes it available to the tree control.
 *
 * The image list should NOT be deleted.
 */
void
ShellTree::EnableImages()
{
    // Get the handle to the system image list, for our icons
    HIMAGELIST hImageList;
    SHFILEINFO sfi;

    hImageList = (HIMAGELIST)SHGetFileInfo(L"C:\\", 
                    0, &sfi, sizeof(SHFILEINFO), 
                    SHGFI_SYSICONINDEX | SHGFI_SMALLICON);

    // Attach ImageList to TreeView
    if (hImageList)
        ::SendMessage(m_hWnd, TVM_SETIMAGELIST, (WPARAM) TVSIL_NORMAL,
            (LPARAM)hImageList);
}

/****************************************************************************
*
*   FUNCTION:   GetSelectedFolderPath(CString &szFolderPath)
*
*   PURPOSE:    Retrieves the path of the currently selected string.
*               Pass a CString object that will hold the folder path. 
*               If the path is not in the filesystem(eg MyComputer) 
*               or none is selected it returns false.
*
*   MESSAGEMAP: NONE
*
****************************************************************************/
BOOL ShellTree::GetSelectedFolderPath(CString &szFolderPath)
{
    TVItemData*     lptvid;  //Long pointer to TreeView item data
    LPSHELLFOLDER   lpsf2=NULL;
    WCHAR           szBuff[MAX_PATH];
    HTREEITEM       hItem=NULL;
    HRESULT         hr;
    BOOL            bRet=false;

    hItem = GetSelectedItem();
    if(hItem)
    {
        lptvid = (TVItemData*)GetItemData(hItem);

        if (lptvid && lptvid->lpsfParent && lptvid->lpi)
        {
            hr = lptvid->lpsfParent->BindToObject(lptvid->lpi,
                     0, IID_IShellFolder,(LPVOID *)&lpsf2);

            if (SUCCEEDED(hr))
            {
                ULONG ulAttrs = SFGAO_FILESYSTEM;

                // Determine what type of object we have.
                lptvid->lpsfParent->GetAttributesOf(1,
                    (const struct _ITEMIDLIST **)&lptvid->lpi, &ulAttrs);

                if (ulAttrs & (SFGAO_FILESYSTEM))
                {
                    if(SHGetPathFromIDList(lptvid->lpifq, szBuff)){
                        szFolderPath = szBuff;
                        bRet = true;
                    }
                }
            }

        }
        if(lpsf2)
            lpsf2->Release();
    }
    return bRet;
}

/****************************************************************************
*
*   FUNCTION:   GetParentShellFolder(HTREEITEM folderNode)
*
*   PURPOSE:    Retrieves the pointer to the ISHELLFOLDER interface
*               of the tree node passed as the paramter.
*
*   MESSAGEMAP: NONE
*
****************************************************************************/
LPSHELLFOLDER ShellTree::GetParentShellFolder(HTREEITEM folderNode)
{
    TVItemData* lptvid;  //Long pointer to TreeView item data

    lptvid = (TVItemData*)GetItemData(folderNode);
    if (lptvid)
        return lptvid->lpsfParent;
    else
        return NULL;
}

/****************************************************************************
*
*   FUNCTION:   GetRelativeIDLIST(HTREEITEM folderNode)
*
*   PURPOSE:    Retrieves the Pointer to an ITEMIDLIST structure that
*               identifies the subfolder relative to its parent folder.
*               see GetParentShellFolder();
*
*   MESSAGEMAP: NONE
*
****************************************************************************/
LPITEMIDLIST ShellTree::GetRelativeIDLIST(HTREEITEM folderNode)
{
    TVItemData* lptvid;  //Long pointer to TreeView item data

    lptvid = (TVItemData*)GetItemData(folderNode);
    if (lptvid)
        return lptvid->lpifq;
    else
        return NULL;
}

/****************************************************************************
*
*   FUNCTION:   GetFullyQualifiedIDLIST(HTREEITEM folderNode)
*
*   PURPOSE:    Retrieves the Pointer to an ITEMIDLIST
*               structure that identifies the subfolder relative to the
*               desktop. This is a fully qualified Item Identifier
*
*   MESSAGEMAP: NONE
*
****************************************************************************/
LPITEMIDLIST ShellTree::GetFullyQualifiedID(HTREEITEM folderNode)
{
    TVItemData* lptvid;  //Long pointer to TreeView item data

    lptvid = (TVItemData*)GetItemData(folderNode);
    if (lptvid)
        return lptvid->lpifq;
    else
        return NULL;
}


/*
 * Tunnel into the tree, finding the node that corresponds to the
 * requested pathname.
 *
 * Sets "resultMsg" to a non-empty string on error.
 */
void
ShellTree::TunnelTree(CString path, CString* pResultStr)
{
    const WCHAR* str = path;
    int len;

    if (str[0] == '\\' && str[1] == '\\') {
        *pResultStr = "Can't expand network locations directly.";
        return;
    }
    len = path.GetLength();
    if (len < 1) {
        *pResultStr = "You must enter a folder name.";
        return;
    }

    /* make sure it ends in \ so splitpath knows it's a directory */
    if (path[len-1] != '\\')
        path += '\\';

    /* if it doesn't exist, there's not much point in searching for it */
    PathName pathName(path);
    if (!pathName.Exists()) {
        *pResultStr = L"Folder not found.";
        return;
    }

    /*
     * Find the folder that corresponds to "My Computer", and then scan
     * it for the drive letter.
     */
    HTREEITEM myComputer = FindMyComputer();
    if (myComputer == NULL) {
        *pResultStr = L"Unable to locate My Computer in tree.";
        return;
    }

    CString drive = pathName.GetDriveOnly();
    LOGI("Searching for drive='%ls'", drive);

    HTREEITEM node = FindDrive(myComputer, drive);
    if (node == NULL) {
        /* unexpected -- couldn't find the drive */
        pResultStr->Format(L"Unable to find drive %ls.", drive);
        return;
    }

    /*
     * We've got the node for the drive.  Now we just need to walk
     * through the tree one level at a time, comparing the name in
     * the tree against our pathname component.
     */
    node = SearchTree(node, pathName.GetPathOnly());

    if (node == NULL) {
        /* unexpected -- file doesn't exist */
        pResultStr->Format(L"Unable to find file '%ls'.",
            (LPCWSTR) pathName.GetPathOnly());
    } else {
        Select(node, TVGN_CARET);
        EnsureVisible(node);
    }
}

/*
 * Find the tree entry that corresponds to "My Computer".
 *
 * This is hampered somewhat by the absence of a way to compare two
 * shell folders for equality.  The PIDL compare function is meant for
 * sorting only (at least as far as it has been documented), and the My
 * Computer "folder" has no path to examine.
 *
 * It helps greatly to assume that My Computer is right under Desktop.
 * If it moved, or if we started the tree somewhere other than right at
 * the desktop, we'd have to recursively search the tree.
 *
 * Returns a handle to the tree item, or NULL if My Computer wasn't found
 * or didn't have any children.
 */
HTREEITEM
ShellTree::FindMyComputer(void)
{
    LPSHELLFOLDER desktop = NULL;
    LPITEMIDLIST myComputerPidl = NULL;
    LPMALLOC lpMalloc = NULL;
    HTREEITEM node;
    HTREEITEM result = NULL;
    HRESULT hr;

    hr = ::SHGetMalloc(&lpMalloc);
    if (FAILED(hr))
       return NULL;

    hr = SHGetDesktopFolder(&desktop);
    if (FAILED(hr))
        goto bail;

    hr = SHGetSpecialFolderLocation(NULL, CSIDL_DRIVES, &myComputerPidl);
    if (FAILED(hr))
        goto bail;

    node = GetRootItem();
    while (node != NULL) {
        CString itemText = GetItemText(node);
        TVItemData* pData = (TVItemData*) GetItemData(node);
        ASSERT(pData != NULL);

        hr = desktop->CompareIDs(0, myComputerPidl, pData->lpi);
        if (SUCCEEDED(hr) && HRESULT_CODE(hr) == 0) {
            LOGI("MATCHED on '%ls'", itemText);
            result = node;
            break;
        }
        node = GetNextSiblingItem(node);
    }

    if (result != NULL && !ItemHasChildren(result)) {
        LOGI("Glitch: My Computer has no children");
        result = NULL;
    }

bail:
    if (desktop != NULL)
        desktop->Release();
    lpMalloc->Free(myComputerPidl);
    lpMalloc->Release();
    return result;
}

/*
 * Given a pointer to the My Computer node in the tree, find the node
 * corresponding to the requested drive (which should be of the form
 * "C:").
 *
 * Returns a pointer to the drive's node on success, or NULL on failure.
 */
HTREEITEM
ShellTree::FindDrive(HTREEITEM myComputer, const CString& drive)
{
    CString udrive;

    /* expand & scan */
    Expand(myComputer, TVE_EXPAND);

    HTREEITEM node;
    node = GetChildItem(myComputer);
    if (node == NULL) {
        ASSERT(false);  // we verified My Computer has kids earlier
        return NULL;
    }

    /*
     * Look for the drive letter.  It's buried amongst other fluff, so
     * we have to rely on Windows preventing the use of a ":" anywhere
     * else in the string to avoid false-positives.
     *
     * We *might* be able to assume it looks like "(C:)", but that's
     * probably unwise.
     */
    udrive = drive;
    udrive.MakeUpper();
    while (node != NULL) {
        CString itemText = GetItemText(node);
        itemText.MakeUpper();

        //LOGI("COMPARING '%ls' vs '%ls'", (LPCWSTR) udrive, (LPCWSTR) itemText);
        if (itemText.Find(udrive) != -1) {
            LOGI("MATCHED '%ls' in '%ls'", (LPCWSTR) udrive, (LPCWSTR) itemText);
            break;
        }
        node = GetNextSiblingItem(node);
    }

    return node;
}

/*
 * Given a path, search a subtree following the components.
 *
 * Pass in the tree's root (it's children will be searched for a
 * match with the first path component) and the path to look for
 * (which must start and end with '\\').
 */
HTREEITEM
ShellTree::SearchTree(HTREEITEM treeNode, const CString& path)
{
    LOGI("SearchTree node=0x%08lx path='%ls'",
        treeNode, (LPCTSTR) path);

    HTREEITEM node;
    CString mangle(path);
    WCHAR* start;
    WCHAR* end;

    /* make a copy of "path" that we can mess with */
    start = mangle.GetBuffer(0);
    if (start == NULL || *start != '\\' || *(start + wcslen(start)-1) != '\\')
        return NULL;
    start++;

    node = treeNode;
    while (*start != '\0') {
        /* grab first node in next level down */
        Expand(node, TVE_EXPAND);   // need to fill in the tree
        node = GetChildItem(node);

        end = wcschr(start, '\\');
        if (end == NULL) {
            ASSERT(false);
            return NULL;
        }
        *end = '\0';

        while (node != NULL) {
            CString itemText = GetItemText(node);

            //LOGI("COMPARE '%s' '%s'", start, itemText);
            if (itemText.CompareNoCase(start) == 0) {
                //LOGI("MATCHED '%s' '%s'", itemText, start);
                break;
            }

            node = GetNextSiblingItem(node);
        }
        if (node == NULL) {
            LOGI("NOT FOUND '%ls' '%ls'", (LPCTSTR) path, start);
            break;
        }

        start = end+1;
    }

    return node;
}


#ifdef USE_OLD

/****************************************************************************
*
*   FUNCTION:   SearchTree( HTREEITEM treeNode,
*                           CString szSearchName )
*
*   PURPOSE:    Too crude to explain, just use it
*
*   WARNING:    Only works if you use the default PopulateTree()
*               Not guaranteed to work on any future or existing
*               version of windows. Use with caution. Pretty much
*               ok if you're using on local drives
*
****************************************************************************/
bool ShellTree::SearchTree(HTREEITEM treeNode,
                            CString szSearchName,
                            FindAttribs attr)
{
    TVItemData* lptvid;  //Long pointer to TreeView item data
    LPSHELLFOLDER   lpsf2=NULL;
    char    drive[_MAX_DRIVE];
    char    dir[_MAX_DIR];
    char    fname[_MAX_FNAME];
    char    ext[_MAX_EXT];
    bool    bRet=false;
    HRESULT hr;
    CString szCompare;

    szSearchName.MakeUpper();
    while(treeNode && bRet==false)
    {
        lptvid=(TVItemData*)GetItemData(treeNode);
        if (lptvid && lptvid->lpsfParent && lptvid->lpi)
        {
            hr=lptvid->lpsfParent->BindToObject(lptvid->lpi,
                     0,IID_IShellFolder,(LPVOID *)&lpsf2);
            if (SUCCEEDED(hr))
            {
                ULONG ulAttrs = SFGAO_FILESYSTEM;
                lptvid->lpsfParent->GetAttributesOf(1,
                    (const struct _ITEMIDLIST **)&lptvid->lpi, &ulAttrs);
                if (ulAttrs & (SFGAO_FILESYSTEM))
                {
                    if(SHGetPathFromIDList(lptvid->lpifq,
                        szCompare.GetBuffer(MAX_PATH)))
                    {
                        switch(attr)
                        {
                        case type_drive:
                            _splitpath(szCompare,drive,dir,fname,ext);
                            szCompare=drive;
                            break;
                        case type_folder:
                            szCompare = GetItemText(treeNode);
                            break;
                        }
                        szCompare.MakeUpper();
                        if(szCompare == szSearchName)
                        {
                            EnsureVisible(treeNode);
                            SelectItem(treeNode);
                            bRet=true;
                        }
                    }
                }
                lpsf2->Release();
            }
        }
        treeNode = GetNextSiblingItem(treeNode);
    }
    return bRet;
}

/****************************************************************************
*
*   FUNCTION:   TunnelTree(CString szFindPath)
*
*   PURPOSE:    Too crude to explain, just use it
*
*   WARNING:    Only works if you use the default PopulateTree()
*               Not guaranteed to work on any future or existing
*               version of windows. Use with caution. Pretty much
*               ok if you're using on local drives
*
****************************************************************************/
void ShellTree::TunnelTree(CString szFindPath)
{
    HTREEITEM subNode = GetRootItem();
    CString szPathHop;
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
    char fname[_MAX_FNAME];
    char ext[_MAX_EXT];
    char delimiter[]="\\";

    PathName checkPath(szFindPath);
    if(!checkPath.Exists())
    {
        MessageBox(szFindPath,"Folder not found",MB_ICONERROR);
        return;
    }
        
    if(szFindPath.ReverseFind('\\') != szFindPath.GetLength()-1)
    {
        szFindPath += "\\";
    }

    _splitpath(szFindPath,drive,dir,fname,ext);

    //search the drive first
    szPathHop=drive;
    subNode=GetChildItem(subNode);
    if(subNode)
    {
        if(SearchTree(subNode,szPathHop, ShellTree::type_drive))
        {
            //break down subfolders and search
            char *p=strtok(dir,delimiter);
            while(p)
            {
                subNode = GetSelectedItem();
                subNode = GetChildItem(subNode);
                if(SearchTree(subNode,p,ShellTree::type_folder))
                    p=strtok(NULL,delimiter);
                else
                    p=NULL;
            }
        }
    }
}


#endif
#ifdef USE_NEW
// new version for Win2K

/****************************************************************************
*
*   FUNCTION:   SearchTree( HTREEITEM treeNode,
*                           CString szSearchName )
*
*   PURPOSE:    Too crude to explain, just use it
*
*   WARNING:    Only works if you use the default PopulateTree()
*               Not guaranteed to work on any future or existing
*               version of windows. Use with caution. Pretty much
*               ok if you're using on local drives
*
****************************************************************************/
bool ShellTree::SearchTree(HTREEITEM treeNode,
                            CString szSearchName,
                            FindAttribs attr)
{
    TVItemData* lptvid;  //Long pointer to TreeView item data
    LPSHELLFOLDER   lpsf2=NULL;
    char    drive[_MAX_DRIVE];
    char    dir[_MAX_DIR];
    char    fname[_MAX_FNAME];
    char    ext[_MAX_EXT];
    bool    bRet=false;
    HRESULT hr;
    CString szCompare;

    szSearchName.MakeUpper();
    while(treeNode && bRet==false)
    {
        lptvid=(TVItemData*)GetItemData(treeNode);
        if (lptvid && lptvid->lpsfParent && lptvid->lpi)
        {
            hr=lptvid->lpsfParent->BindToObject(lptvid->lpi,
                     0,IID_IShellFolder,(LPVOID *)&lpsf2);
            if (SUCCEEDED(hr))
            {
                ULONG ulAttrs = SFGAO_FILESYSTEM;
                lptvid->lpsfParent->GetAttributesOf(1, (const struct _ITEMIDLIST **)&lptvid->lpi, &ulAttrs);
                if (ulAttrs & (SFGAO_FILESYSTEM))
                {
                    if(SHGetPathFromIDList(lptvid->lpifq,
                        szCompare.GetBuffer(MAX_PATH)))
                    {
                        CString folder;

                        SHGetSpecialFolderPath(NULL, 
                            folder.GetBuffer(MAX_PATH), 
                            CSIDL_COMMON_DESKTOPDIRECTORY, FALSE);  
                        if( szCompare.Find( folder ) != -1 )
                            if( szSearchName.Find( szCompare ) == -1 ) {
                                LOGI("Magic match on '%s'", szCompare);
                                return false;
                            }

                        SHGetSpecialFolderPath(NULL,
                            folder.GetBuffer(MAX_PATH),
                            CSIDL_DESKTOPDIRECTORY, FALSE );    
                        if( szCompare.Find( folder ) != -1 )
                            if( szSearchName.Find( szCompare ) == -1 ) {
                                LOGI("MAGIC '%s'='%s' and '%s'='%s'",
                                    szCompare, folder, szSearchName, szCompare);
                                return false;
                            }

                        SHGetSpecialFolderPath(NULL,
                            folder.GetBuffer(MAX_PATH),
                            CSIDL_PERSONAL, FALSE ); 
                        if( szCompare.Find( folder ) != -1 )
                            if( szSearchName.Find( szCompare ) == -1 ) {
                                LOGI("Magic match on '%s'", szCompare);
                                return false;
                            }

                        switch(attr) {
                        case type_drive:
                            _splitpath(szCompare,drive,dir,fname,ext);
                            szCompare = drive;
                            break;
                        case type_folder:
                            szCompare = GetItemText(treeNode);
                            break;
                        }
                        szCompare.MakeUpper();
                        if(szCompare == szSearchName)
                        {
                            EnsureVisible(treeNode);
                            SelectItem(treeNode);
                            bRet=true;
                        }
                    }
                }
                lpsf2->Release();
            }
        }
        treeNode = GetNextSiblingItem(treeNode);
    }
    return bRet;
}

/****************************************************************************
*
*   FUNCTION:   TunnelTree(CString szFindPath)
*
*   PURPOSE:    Too crude to explain, just use it
*
*   WARNING:    Only works if you use the default PopulateTree()
*               Not guaranteed to work on any future or existing
*               version of windows. Use with caution. Pretty much
*               ok if you're using on local drives
*
****************************************************************************/
void ShellTree::TunnelTree(CString szFindPath)
{
    HTREEITEM subNode = GetRootItem();
    CString szPathHop;
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
    char fname[_MAX_FNAME];
    char ext[_MAX_EXT];
    char delimiter[]="\\";

    PathName checkPath(szFindPath);
    if(!checkPath.Exists()) {
        MessageBox(szFindPath,"Folder not found",MB_ICONERROR);
        return;
    }
        
    if(szFindPath.ReverseFind('\\') != szFindPath.GetLength()-1) {
        szFindPath += "\\";
    }

    _splitpath(szFindPath, drive, dir, fname, ext);

    HTREEITEM root = subNode;
    //search the drive first
    szPathHop=drive;
    do {
        CString currItem = GetItemText( root );
        LOGI("Scanning '%s' for drive '%s'", currItem, szPathHop);
        if (ItemHasChildren(root))
        {
            Expand(root, TVE_EXPAND);
            subNode = GetChildItem(root);
            if(subNode)
            {
                if(SearchTree(subNode, szPathHop, ShellTree::type_drive))
                {
                    // we have a match on the drive; SearchTree will have
                    // left it as the selected item
                    LOGI("Tunnel match '%s' in subnode", szPathHop);

                    // break down subfolders and search
                    char* p = strtok(dir, delimiter);
                    while (p) {
                        subNode = GetSelectedItem();
                        subNode = GetChildItem(subNode);
                        if(SearchTree(subNode, p, ShellTree::type_folder))
                            p=strtok(NULL,delimiter);
                        else
                            p=NULL;
                    }
                    return;
                }
            }
            Expand(root, TVE_COLLAPSE);
        }

        root = GetNextSiblingItem( root );
    } while( root );
}
#endif



#if 0   // quick test
LPMALLOC g_pMalloc = NULL;

// Main_OnBrowse - browses for a program folder. 
// hwnd - handle to the application's main window. 
// 
// Uses the global variable g_pMalloc, which is assumed to point 
//     to the shell's IMalloc interface. 
void Main_OnBrowse(HWND hwnd) 
{
    BROWSEINFO bi; 
    LPSTR lpBuffer; 
    LPITEMIDLIST pidlPrograms;  // PIDL for Programs folder 
    LPITEMIDLIST pidlBrowse;    // PIDL selected by user 
 
    if (g_pMalloc == NULL)
        ::SHGetMalloc(&g_pMalloc);

    // Allocate a buffer to receive browse information. 
    if ((lpBuffer = (LPSTR) g_pMalloc->Alloc( 
            MAX_PATH)) == NULL) 
        return; 
 
    // Get the PIDL for the Programs folder. 
    if (!SUCCEEDED(SHGetSpecialFolderLocation( 
            hwnd, CSIDL_PROGRAMS, &pidlPrograms))) { 
        g_pMalloc->Free(lpBuffer); 
        return; 
    } 
 
    // Fill in the BROWSEINFO structure. 
    bi.hwndOwner = hwnd; 
    bi.pidlRoot = pidlPrograms; 
    bi.pszDisplayName = lpBuffer; 
    bi.lpszTitle = "Choose a Program Group"; 
    bi.ulFlags = 0; 
    bi.lpfn = NULL; 
    bi.lParam = 0; 
 
    // Browse for a folder and return its PIDL. 
    pidlBrowse = SHBrowseForFolder(&bi); 
    if (pidlBrowse != NULL) { 
 
        // Show the display name, title, and file system path. 
        MessageBox(hwnd, lpBuffer, "Display name", MB_OK); 
        if (SHGetPathFromIDList(pidlBrowse, lpBuffer)) 
            SetWindowText(hwnd, lpBuffer); 
 
        // Free the PIDL returned by SHBrowseForFolder. 
        g_pMalloc->Free(pidlBrowse); 
    } 
 
    // Clean up. 
    g_pMalloc->Free(pidlPrograms); 
    g_pMalloc->Free(lpBuffer); 
}
#endif
