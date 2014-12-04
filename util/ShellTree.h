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
#ifndef UTIL_SHELLTREE_H
#define UTIL_SHELLTREE_H


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

    /*
     * Replace a CTreeCtrl in a dialog box with us.  All of the styles are
     * copied from the original dialog window.
     *
     * Returns TRUE on success, FALSE on failure.
     */
    BOOL ReplaceDlgCtrl(CDialog* pDialog, int treeID);

    /*
     * Populate the tree, starting from "nFolder".
     *
     * Returns TRUE on success, FALSE on failure.
     */
    BOOL PopulateTree(int nFolder = CSIDL_DESKTOP);

    /*
     * Open up and select My Computer.
     */
    void ExpandMyComputer(void);

    /*
     * Add a new folder to the tree at the currently-selected node.  This may
     * not actually add a folder if the new folder is at a point in the tree
     * below where we have already expanded.
     *
     * Returns TRUE on success, or FALSE on failure.
     */
    BOOL AddFolderAtSelection(const CString& name);

    void GetContextMenu(NMHDR* pNMHDR, LRESULT* pResult);

    /*
     * Gets a handle to the system image list (by just grabbing whatever is
     * in place for C:\) and makes it available to the tree control.
     *
     * The image list should NOT be deleted.
     */
    void EnableImages();

    /*
     * Retrieves the path of the currently selected string.
     * Pass a CString object that will hold the folder path. 
     * If the path is not in the filesystem(eg MyComputer) 
     * or none is selected it returns false.
     */
    BOOL GetSelectedFolderPath(CString &szFolderPath);

    /*
     * Retrieves the pointer to the ISHELLFOLDER interface
     * of the tree node passed as the parameter.
     */
    LPSHELLFOLDER GetParentShellFolder(HTREEITEM folderNode);

    /*
     * Retrieves the Pointer to an ITEMIDLIST structure that
     * identifies the subfolder relative to its parent folder.
     * see GetParentShellFolder();
     */
    LPITEMIDLIST GetRelativeIDLIST(HTREEITEM folderNode);

    /*
     * Retrieves the Pointer to an ITEMIDLIST
     * structure that identifies the subfolder relative to the
     * desktop. This is a fully qualified Item Identifier
    */
    LPITEMIDLIST GetFullyQualifiedID(HTREEITEM folderNode);

    /*
     * Tunnel into the tree, finding the node that corresponds to the
     * requested pathname.
     *
     * Sets "resultMsg" to a non-empty string on error.
     */
    void TunnelTree(CString path, CString* pResultStr);

    // Get the most-recently-set folder path.  This will be updated on
    // every TVN_SELCHANGED, so add an ON_NOTIFY handler to the parent.
    BOOL GetFolderPath(CString* pStr) {
        *pStr = fFolderPath;
        return fFolderPathValid;
    }

protected:
    /*
     * Respond to TVN_ITEMEXPANDING message.
     *
     * If the subtree hasn't been expanded yet, dig in.
     */
    void OnFolderExpanding(NMHDR* pNMHDR, LRESULT* pResult);

    /*
     * Handle TVN_DELETEITEM notification by cleaning up our stuff.
     */
    void OnDeleteShellItem(NMHDR* pNMHDR, LRESULT* pResult);

    /*
     * Respond to TVN_SELCHANGED notification.
     */
    BOOL OnSelectionChange(NMHDR* pNMHDR, LRESULT* pResult);

    /*
     * This does the bulk of the work when the selection changes.
     *
     * The filesystem path (if any) to the object is placed in "szFolderPath".
     */
    BOOL OnFolderSelected(NMHDR* pNMHDR, LRESULT* pResult,
        CString& szFolderPath);

    /*
     * Fills a branch of the TreeView control.  Given the shell folder (both as
     * a shell folder and the fully-qualified item ID list to it) and the parent
     * item in the tree (TVI_ROOT to start off), add all the kids to the tree.
     *
     * Does not try to add the current entry, as a result of which we don't
     * have a root "Desktop" node that everything is a child of.  This is okay.
     */
    void FillTreeView(LPSHELLFOLDER lpsf,LPITEMIDLIST lpifq, HTREEITEM hParent);

    /*
     * Add a node to the tree.
     *
     * Returns TRUE on success, FALSE on failure.
     */
    BOOL AddNode(LPSHELLFOLDER lpsf, LPITEMIDLIST lpi, LPITEMIDLIST lpifq,
        unsigned long ulAttrs, HTREEITEM hParent, HTREEITEM* phPrev);

    /*
     * Sort function callback for TreeView SortChildrenCB.
     */
    static int CALLBACK TreeViewCompareProc(LPARAM, LPARAM, LPARAM);

    /*
     * Set the TreeView normal and selected icons for the specified entry.
     *
     * "lpifq" is the fully-qualified PIDL, LPTV_ITEM is an item in the tree.
     */
    void GetNormalAndSelectedIcons(LPITEMIDLIST lpifq, LPTV_ITEM lptvitem);

    /*
     * Find the tree entry that corresponds to "My Computer".
     *
     * Returns a handle to the tree item, or NULL if My Computer wasn't found
     * or didn't have any children.
     */
    HTREEITEM FindMyComputer(void);

    /*
     * Given a pointer to the My Computer node in the tree, find the node
     * corresponding to the requested drive (which should be of the form
     * "C:").
     *
     * Returns a pointer to the drive's node on success, or NULL on failure.
     */
    HTREEITEM FindDrive(HTREEITEM myComputer, const CString& drive);

    /*
     * Given a path, search a subtree following the components.
     *
     * Pass in the tree's root (it's children will be searched for a
     * match with the first path component) and the path to look for
     * (which must start and end with '\\').
     */
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

private:
    DECLARE_COPY_AND_OPEQ(ShellTree)
};

#endif /*UTIL_SHELLTREE_H*/
