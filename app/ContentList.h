/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Declarations for a list control showing archive contents.
 */
#ifndef APP_CONTENTLIST_H
#define APP_CONTENTLIST_H

#include "GenericArchive.h"
#include "Preferences.h"
#include "Resource.h"
#include <afxwin.h>
#include <afxcmn.h>


/*
 * A ListCtrl with headers appropriate for viewing archive contents.
 *
 * NOTE: this class performs auto-cleanup, and must be allocated on the heap.
 *
 * We currently use the underlying GenericArchive as our storage for the stuff
 * we display.  This works great until we change or delete entries from
 * GenericArchive.  At that point we run the risk of displaying bad pointers.
 *
 * The GenericArchive has local copies of everything interesting, so the only
 * time things go badly for us is when somebody inside GenericArchive calls
 * Reload.  That frees and reallocates the storage we're pointing to.  So,
 * GenericArchive maintains a "I have reloaded" flag that we test before we
 * draw.
 */
class ContentList: public CListCtrl
{
public:
    ContentList(GenericArchive* pArchive, ColumnLayout* pLayout) {
        ASSERT(pArchive != NULL);
        ASSERT(pLayout != NULL);
        fpArchive = pArchive;
        fpLayout = pLayout;
//      fInvalid = false;
        //fRightClickItem = -1;

        fpArchive->ClearReloadFlag();
    }

    /*
     * The archive contents have changed.  Reload the list from the
     * GenericArchive.
     *
     * Reloading causes the current selection and view position to be lost.  This
     * is sort of annoying if all we did is add a comment, so we try to save the
     * selection and reapply it.  To do this correctly we need some sort of
     * unique identifier so we can spot the records that have come back.
     *
     * Nothing in GenericArchive should be considered valid at this point.
     */
    void Reload(bool saveSelection = false);

    /*
     * Call this when the sort order changes.
     */
    void NewSortOrder(void);

    /*
     * Call this when the column widths are changed programmatically (e.g. by
     * the preferences page enabling or disabling columns).
     *
     * We want to set any defaulted entries to actual values so that, if the
     * font properties change, column A doesn't resize when column B is tweaked
     * in the Preferences dialog.  (If it's still set to "default", then when
     * we say "update all widths" the defaultedness will be re-evaluated.)
     */
    void NewColumnWidths(void);

    /*
     * Copy the current column widths out to the Preferences object.
     */
    void ExportColumnWidths(void);

    /*
     * Mark everything as selected.
     */
    void SelectAll(void);

    /*
     * Toggle the "selected" state flag.
     */
    void InvertSelection(void);

    /*
     * Mark all items as unselected.
     */
    void ClearSelection(void);

    /*
     * Select the contents of any selected subdirs.
     */
    void SelectSubdirContents(void);

    /*
     * Find the next matching entry.  We start after the first selected item.
     * If we find a matching entry, we clear the current selection and select it.
     */
    void FindNext(const WCHAR* str, bool down, bool matchCase, bool wholeWord);

    /*
     * Compare "str" against the contents of entry "num".
     */
    bool CompareFindString(int num, const WCHAR* str, bool matchCase,
        bool wholeWord);

    //int GetRightClickItem(void) const { return fRightClickItem; }
    //void ClearRightClickItem(void) { fRightClickItem = -1; }

    enum { kFileTypeBufLen = 5, kAuxTypeBufLen = 6 };

    /*
     * Get the file type display string.
     *
     * "buf" must be able to hold at least 4 characters plus the NUL (i.e. 5).
     * Use kFileTypeBufLen.
     */
    static void MakeFileTypeDisplayString(const GenericEntry* pEntry,
        WCHAR* buf);

    /*
     * Get the aux type display string.
     *
     * "buf" must be able to hold at least 5 characters plus the NUL (i.e. 6).
     * Use kAuxTypeBufLen.
     */
    static void MakeAuxTypeDisplayString(const GenericEntry* pEntry,
        WCHAR* buf);

protected:
    /*
     * Puts the window into "report" mode, and add a client edge since we're not
     * using one on the frame window.
     */
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;

    // Destroy "this".
    virtual void PostNcDestroy(void) override;

    /*
     * Create and populate list control.
     */
    afx_msg int OnCreate(LPCREATESTRUCT);

    /*
     * When being shut down, save off the column width info before the window
     * gets destroyed.
     */
    afx_msg void OnDestroy(void);

    /*
     * The system colors are changing; delete the image list and re-load it.
     */
    afx_msg void OnSysColorChange(void);

    /*
     * They've clicked on a header.  Figure out what kind of sort order we want
     * to use.
     */
    afx_msg void OnColumnClick(NMHDR*, LRESULT*);

    /*
     * Return the value for a particular row and column.
     *
     * This gets called *a lot* while the list is being drawn, scrolled, etc.
     * Don't do anything too expensive.
     */
    afx_msg void OnGetDispInfo(NMHDR* pnmh, LRESULT* pResult);

private:
    // Load the header images.  Must do this every time the syscolors change.
    // (Ideally this would re-map all 3dface colors.  Note the current
    // implementation relies on the top left pixel color.)
    void LoadHeaderImages(void) {
        if (!fHdrImageList.Create(IDB_HDRBAR, 16, 1, CLR_DEFAULT))
            LOGW("GLITCH: header list create failed");
        fHdrImageList.SetBkColor(::GetSysColor(COLOR_BTNFACE));
    }
    void LoadListImages(void) {
        if (!fListImageList.Create(IDB_LIST_PICS, 16, 1, CLR_DEFAULT))
            LOGW("GLITCH: list image create failed");
        fListImageList.SetBkColor(::GetSysColor(COLOR_WINDOW));
    }
    enum {  // defs for IDB_LIST_PICS
        kListIconNone = 0,
        kListIconComment = 1,
        kListIconNonEmptyComment = 2,
        kListIconDamaged = 3,
        kListIconSuspicious = 4,
    };

    /*
     * Fill the columns with data from the archive entries.  We use a "virtual"
     * list control to avoid storing everything multiple times.  However, we
     * still create one item per entry so that the list control will do most
     * of the sorting for us (otherwise we have to do the sorting ourselves).
     *
     * Someday we should probably move to a wholly virtual list view.
     */
    int LoadData(void);

    /*
     * Get the "selection serials" from the list of selected items.
     *
     * The caller is responsible for delete[]ing the return value.
     */
    long* GetSelectionSerials(long* pSelCount);

    /*
     * Restore the selection from the "savedSel" list.
     */
    void RestoreSelection(const long* savedSel, long selCount);

    /*
     * Return the default width for the specified column.
     */
    int GetDefaultWidth(int col);

    /*
     * Convert an HFS file/creator type into a string.
     *
     * "buf" must be able to hold at least 4 characters plus the NUL.  Use
     * kFileTypeBufLen.
     */
    static void MakeMacTypeString(unsigned long val, WCHAR* buf);

    /*
     * Generate the funky ratio display string.  While we're at it, return a
     * numeric value that we can sort on.
     *
     * "buf" must be able to hold at least 6 chars plus the NULL.
     */
    static void MakeRatioDisplayString(const GenericEntry* pEntry, WCHAR* buf,
        int* pPerc);

    /*
     * Set the up/down sorting arrow as appropriate.
     */
    void SetSortIcon(void);

    /*
     * Static comparison function for list sorting.
     */
    static int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2,
        LPARAM lParamSort);

    /*
     * Handle a double-click on an item.
     *
     * The double-click should single-select the item, so we can throw it
     * straight into the viewer.  However, there are some uses for bulk
     * double-clicking.
     */
    void OnDoubleClick(NMHDR* pnmh, LRESULT* pResult);

    /*
     * Handle a right-click on an item.
     */
    void OnRightClick(NMHDR* pnmh, LRESULT* pResult);

    /*
     * Select every entry whose display name has "displayPrefix" as a prefix.
     */
    void SelectSubdir(const WCHAR* displayPrefix);

    CImageList      fHdrImageList;
    CImageList      fListImageList;
    GenericArchive* fpArchive;  // data we're expected to display
    ColumnLayout*   fpLayout;
//  int             fRightClickItem;
//  bool            fInvalid;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_CONTENTLIST_H*/
