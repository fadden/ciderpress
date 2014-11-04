/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * TreeView control containing Windows shell folders.
 *
 * Originally based on MFCENUM from "Programming the Windows 95 User
 * interface".  Enhanced by Selom Ofori as "ShellTree" class.  Modified
 * extensively.
 */
#ifndef __SHELLTREE__
#define __SHELLTREE__


/*
 * ShellTree class.
 */
class ShellTree : public CTreeCtrl {
public:
    //enum FindAttribs {type_drive, type_folder};

    ShellTree(void) {
        fFolderPathValid = false;
    }
    virtual ~ShellTree(void) {
        Detach();       // we don't own the window handle
    }

    BOOL ReplaceDlgCtrl(CDialog* pDialog, int treeID);
    BOOL PopulateTree(int nFolder = CSIDL_DESKTOP);
    void ExpandMyComputer(void);
    BOOL AddFolderAtSelection(const CString& name);
    void GetContextMenu(NMHDR* pNMHDR, LRESULT* pResult);
    void EnableImages();
    BOOL GetSelectedFolderPath(CString &szFolderPath);
    void TunnelTree(CString path, CString* pResultStr);
    LPSHELLFOLDER   GetParentShellFolder(HTREEITEM folderNode);
    LPITEMIDLIST    GetRelativeIDLIST(HTREEITEM folderNode);
    LPITEMIDLIST    GetFullyQualifiedID(HTREEITEM folderNode);

    // Get the most-recently-set folder path.  This will be updated on
    // every TVN_SELCHANGED, so add an ON_NOTIFY handler to the parent.
    BOOL GetFolderPath(CString* pStr) {
        *pStr = fFolderPath;
        return fFolderPathValid;
    }

protected:
    void OnFolderExpanding(NMHDR* pNMHDR, LRESULT* pResult);
    void OnDeleteShellItem(NMHDR* pNMHDR, LRESULT* pResult);
    BOOL OnFolderSelected(NMHDR* pNMHDR, LRESULT* pResult,
        CString& szFolderPath);
    BOOL OnSelectionChange(NMHDR* pNMHDR, LRESULT* pResult);

    void FillTreeView(LPSHELLFOLDER lpsf,LPITEMIDLIST lpifq, HTREEITEM hParent);
    BOOL AddNode(LPSHELLFOLDER lpsf, LPITEMIDLIST lpi, LPITEMIDLIST lpifq,
        unsigned long ulAttrs, HTREEITEM hParent, HTREEITEM* phPrev);
    static int CALLBACK TreeViewCompareProc(LPARAM, LPARAM, LPARAM);
    void GetNormalAndSelectedIcons(LPITEMIDLIST lpifq, LPTV_ITEM lptvitem);
    HTREEITEM FindMyComputer(void);
    HTREEITEM FindDrive(HTREEITEM myComputer, const CString& drive);
    HTREEITEM SearchTree(HTREEITEM treeNode, const CString& path);
    
    /*
     * Tree view element.  Each one holds a pointer to the ShellFolder
     * object, a pointer to the ItemIDList for the item within the folder,
     * and a pointer to the fully-qualified ItemIDList for the item.
     */
    typedef struct TVItemData {
        LPSHELLFOLDER   lpsfParent;
        LPITEMIDLIST    lpi;
        LPITEMIDLIST    lpifq;
        //bool          alphaSort;
    } TVItemData;

    CString fFolderPath;
    BOOL    fFolderPathValid;

    DECLARE_MESSAGE_MAP()
};

#endif /*__SHELLTREE__*/