/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Class declaration for a list control showing archive contents.
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

    // call this before updating underlying storage; call Reload to un-inval
//  void Invalidate(void);
    // reload from underlying storage
    void Reload(bool saveSelection = false);

    void NewSortOrder(void);
    void NewColumnWidths(void);
    void ExportColumnWidths(void);
    void SelectAll(void);
    void InvertSelection(void);
    void ClearSelection(void);

    void SelectSubdirContents(void);

    void FindNext(const WCHAR* str, bool down, bool matchCase, bool wholeWord);
    bool CompareFindString(int num, const WCHAR* str, bool matchCase,
        bool wholeWord);

    //int GetRightClickItem(void) const { return fRightClickItem; }
    //void ClearRightClickItem(void) { fRightClickItem = -1; }

    enum { kFileTypeBufLen = 5, kAuxTypeBufLen = 6 };
    static void MakeFileTypeDisplayString(const GenericEntry* pEntry,
        WCHAR* buf);
    static void MakeAuxTypeDisplayString(const GenericEntry* pEntry,
        WCHAR* buf);

protected:
    // overridden functions
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual void PostNcDestroy(void);

    afx_msg int OnCreate(LPCREATESTRUCT);
    afx_msg void OnDestroy(void);
    afx_msg void OnSysColorChange(void);
    //afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnColumnClick(NMHDR*, LRESULT*);
    afx_msg void OnGetDispInfo(NMHDR* pnmh, LRESULT* pResult);

private:
    // Load the header images.  Must do this every time the syscolors change.
    // (Ideally this would re-map all 3dface colors.  Note the current
    // implementation relies on the top left pixel color.)
    void LoadHeaderImages(void) {
        if (!fHdrImageList.Create(IDB_HDRBAR, 16, 1, CLR_DEFAULT))
            LOGI("GLITCH: header list create failed");
        fHdrImageList.SetBkColor(::GetSysColor(COLOR_BTNFACE));
    }
    void LoadListImages(void) {
        if (!fListImageList.Create(IDB_LIST_PICS, 16, 1, CLR_DEFAULT))
            LOGI("GLITCH: list image create failed");
        fListImageList.SetBkColor(::GetSysColor(COLOR_WINDOW));
    }
    enum {  // defs for IDB_LIST_PICS
        kListIconNone = 0,
        kListIconComment = 1,
        kListIconNonEmptyComment = 2,
        kListIconDamaged = 3,
        kListIconSuspicious = 4,
    };
    int LoadData(void);
    long* GetSelectionSerials(long* pSelCount);
    void RestoreSelection(const long* savedSel, long selCount);

    int GetDefaultWidth(int col);

    static void MakeMacTypeString(unsigned long val, WCHAR* buf);
    static void MakeRatioDisplayString(const GenericEntry* pEntry, WCHAR* buf,
        int* pPerc);

    void SetSortIcon(void);
    static int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2,
        LPARAM lParamSort);

    void OnDoubleClick(NMHDR* pnmh, LRESULT* pResult);
    void OnRightClick(NMHDR* pnmh, LRESULT* pResult);
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
