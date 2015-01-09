/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of list control showing archive contents.
 */
#include "stdafx.h"
#include "Main.h"
#include "ContentList.h"

const LPARAM kDescendingFlag = 0x0100;


BEGIN_MESSAGE_MAP(ContentList, CListCtrl)
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_SYSCOLORCHANGE()
    //ON_WM_MOUSEWHEEL()
    ON_NOTIFY_REFLECT(NM_DBLCLK, OnDoubleClick)
    ON_NOTIFY_REFLECT(NM_RCLICK, OnRightClick)
    ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnColumnClick)
    ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnGetDispInfo)
END_MESSAGE_MAP()

#if 0
afx_msg BOOL
ContentList::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    LOGI("MOUSE WHEEL");
    return CWnd::OnMouseWheel(nFlags, zDelta, pt);
//  return TRUE;
}
#endif


BOOL ContentList::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CListCtrl::PreCreateWindow(cs))
        return FALSE;

    cs.style &= ~LVS_TYPEMASK;
    cs.style |= LVS_REPORT;
    cs.style |= LVS_SHOWSELALWAYS;
    cs.dwExStyle |= WS_EX_CLIENTEDGE;

    return TRUE;
}

void ContentList::PostNcDestroy(void)
{
    LOGI("ContentList PostNcDestroy");
    delete this;
}

static inline int MaxVal(int a, int b)
{
    return a > b ? a : b;
}

int ContentList::OnCreate(LPCREATESTRUCT lpcs)
{
    CString colHdrs[kNumVisibleColumns] = {
        L"Pathname", L"Type", L"Aux", L"Mod Date",
        L"Format", L"Size", L"Ratio", L"Packed", L"Access"
    };  // these should come from string table, not hard-coded
    static int colFmt[kNumVisibleColumns] = {
        LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT,
        LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_RIGHT, LVCFMT_RIGHT, LVCFMT_LEFT
    };

    if (CListCtrl::OnCreate(lpcs) == -1)
        return -1;

    /*
     * Create all of the columns with an initial width of 1, then set
     * them to the correct values with NewColumnWidths() (which handles
     * defaulted values).
     */
    for (int i = 0; i < kNumVisibleColumns; i++)
        InsertColumn(i, colHdrs[i], colFmt[i], 1);
    NewColumnWidths();

    /* add images for list; this MUST be loaded before header images */
    LoadListImages();
    SetImageList(&fListImageList, LVSIL_SMALL);

    /* add our up/down arrow bitmaps */
    LoadHeaderImages();
    CHeaderCtrl* pHeader = GetHeaderCtrl();
    if (pHeader == NULL)
        LOGW("GLITCH: couldn't get header ctrl");
    ASSERT(pHeader != NULL);
    pHeader->SetImageList(&fHdrImageList);

    /* load the data and sort it */
    if (LoadData() != 0) {
        MessageBox(L"Not all entries were loaded.", L"Error",
            MB_OK | MB_ICONSTOP);
        /* keep going with what we've got; the error only affects display */
    }
    NewSortOrder();

    /* grab the focus so we get keyboard and mouse wheel messages */
    SetFocus();

    /* highlight/select entire line, not just filename */
    ListView_SetExtendedListViewStyleEx(m_hWnd,
        LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

    return 0;
}

void ContentList::OnDestroy(void)
{
    LOGD("ContentList OnDestroy");

    ExportColumnWidths();
    CListCtrl::OnDestroy();
}

void ContentList::OnSysColorChange(void)
{
    fHdrImageList.DeleteImageList();
    LoadHeaderImages();
}

void ContentList::OnColumnClick(NMHDR* pnmh, LRESULT* pResult)
{
    NM_LISTVIEW* pnmlv = (NM_LISTVIEW*) pnmh;

    LOGD("ContentList OnColumnClick");

    if (fpLayout->GetSortColumn() == pnmlv->iSubItem)
        fpLayout->SetAscending(!fpLayout->GetAscending());
    else {
        fpLayout->SetSortColumn(pnmlv->iSubItem);
        fpLayout->SetAscending(true);
    }

    NewSortOrder();
    *pResult = 0;
}

void ContentList::ExportColumnWidths(void)
{
    //LOGI("ExportColumnWidths");
    for (int i = 0; i < kNumVisibleColumns; i++)
        fpLayout->SetColumnWidth(i, GetColumnWidth(i));
}

void ContentList::NewColumnWidths(void)
{
    for (int i = 0; i < kNumVisibleColumns; i++) {
        int width = fpLayout->GetColumnWidth(i);
        if (width == ColumnLayout::kWidthDefaulted) {
            width = GetDefaultWidth(i);
            LOGD("Defaulting width %d to %d", i, width);
            fpLayout->SetColumnWidth(i, width);
        }
        SetColumnWidth(i, width);
    }
}

void ContentList::Reload(bool saveSelection)
{
    LOGI("Reloading ContentList");
    CWaitCursor waitc;

//  fInvalid = false;
    fpArchive->ClearReloadFlag();

    long* savedSel = NULL;
    long selCount = 0;

    if (saveSelection) {
        /* get the serials for the current selection (if any) */
        savedSel = GetSelectionSerials(&selCount);
    }

    /* get the item that's currently at the top of the page */
    int top = GetTopIndex();
    int bottom = top + GetCountPerPage() -1;

    /* reload the list */
    LoadData();
    NewSortOrder();

    if (savedSel != NULL) {
        /* restore the selection */
        RestoreSelection(savedSel, selCount);
        delete[] savedSel;
    }

    /* try to put us back in the same place */
    EnsureVisible(bottom, false);
    EnsureVisible(top, false);
}

long* ContentList::GetSelectionSerials(long* pSelCount)
{
    long* savedSel = NULL;
    long maxCount;

    maxCount = GetSelectedCount();
    LOGD("GetSelectionSerials (maxCount=%d)", maxCount);

    if (maxCount > 0) {
        savedSel = new long[maxCount];
        int idx = 0;

        POSITION posn;
        posn = GetFirstSelectedItemPosition();
        ASSERT(posn != NULL);
        if (posn == NULL)
            return NULL;
        while (posn != NULL) {
            int num = GetNextSelectedItem(posn);
            GenericEntry* pEntry = (GenericEntry*) GetItemData(num);

            if (idx == maxCount) {
                ASSERT(false);
                break;
            }
            savedSel[idx++] = pEntry->GetSelectionSerial();
        }

        ASSERT(idx == maxCount);
    }

    *pSelCount = maxCount;
    return savedSel;
}

void ContentList::RestoreSelection(const long* savedSel, long selCount)
{
    LOGI("RestoreSelection (selCount=%d)", selCount);
    if (savedSel == NULL)
        return;

    int i, j;

    for (i = GetItemCount()-1; i >= 0; i--) {
        GenericEntry* pEntry = (GenericEntry*) GetItemData(i);

        for (j = 0; j < selCount; j++) {
            if (pEntry->GetSelectionSerial() == savedSel[j] &&
                pEntry->GetSelectionSerial() != -1)
            {
                /* match! */
                if (SetItemState(i, LVIS_SELECTED, LVIS_SELECTED) == FALSE) {
                    LOGW("WHOA: unable to set selected on item=%d", i);
                }
                break;
            }
        }
    }
}

void ContentList::NewSortOrder(void)
{
    CWaitCursor wait;       // automatically changes mouse to hourglass
    int column;

    column = fpLayout->GetSortColumn();
    if (!fpLayout->GetAscending())
        column |= kDescendingFlag;

    SetSortIcon();
    SortItems(CompareFunc, column);
}

/*static*/ void ContentList::MakeFileTypeDisplayString(const GenericEntry* pEntry,
    WCHAR* buf)
{
    bool isDir =
        pEntry->GetRecordKind() == GenericEntry::kRecordKindVolumeDir ||
        pEntry->GetRecordKind() == GenericEntry::kRecordKindDirectory;

    if (pEntry->GetSourceFS() == DiskImg::kFormatMacHFS && isDir) {
        /* HFS directories don't have types; fake it */
        wcscpy(buf, L"DIR/");
    } else if (!(pEntry->GetFileType() >= 0 && pEntry->GetFileType() <= 0xff))
    {
        /* oversized type; assume it's HFS */
        WCHAR typeBuf[kFileTypeBufLen];
        MakeMacTypeString(pEntry->GetFileType(), typeBuf);

        switch (pEntry->GetRecordKind()) {
        case GenericEntry::kRecordKindFile:
            wcscpy(buf, typeBuf);
            break;
        case GenericEntry::kRecordKindForkedFile:
            wsprintf(buf, L"%ls+", typeBuf);
            break;
        case GenericEntry::kRecordKindUnknown:
            // shouldn't happen
            wsprintf(buf, L"%ls-", typeBuf);
            break;
        case GenericEntry::kRecordKindVolumeDir:
        case GenericEntry::kRecordKindDirectory:
        case GenericEntry::kRecordKindDisk:
        default:
            ASSERT(FALSE);
            wcscpy(buf, L"!!!");
            break;
        }
    } else {
        /* typical ProDOS-style stuff */
        switch (pEntry->GetRecordKind()) {
        case GenericEntry::kRecordKindVolumeDir:
        case GenericEntry::kRecordKindDirectory:
            wsprintf(buf, L"%ls/", pEntry->GetFileTypeString());
            break;
        case GenericEntry::kRecordKindFile:
            wsprintf(buf, L"%ls", pEntry->GetFileTypeString());
            break;
        case GenericEntry::kRecordKindForkedFile:
            wsprintf(buf, L"%ls+", pEntry->GetFileTypeString());
            break;
        case GenericEntry::kRecordKindDisk:
            wcscpy(buf, L"Disk");
            break;
        case GenericEntry::kRecordKindUnknown:
            // usually a GSHK-archived empty data file does this
            wsprintf(buf, L"%ls-", pEntry->GetFileTypeString());
            break;
        default:
            ASSERT(FALSE);
            wcscpy(buf, L"!!!");
            break;
        }
    }
}

/*static*/ void ContentList::MakeMacTypeString(unsigned long val, WCHAR* buf)
{
    /* expand longword with ASCII type bytes */
    buf[0] = (unsigned char) (val >> 24);
    buf[1] = (unsigned char) (val >> 16);
    buf[2] = (unsigned char) (val >> 8);
    buf[3] = (unsigned char) val;
    buf[4] = '\0';

    /* sanitize */
    while (*buf != '\0') {
        *buf = DiskImg::MacToASCII((unsigned char)*buf);
        buf++;
    }
}

/*static*/ void ContentList::MakeAuxTypeDisplayString(const GenericEntry* pEntry,
    WCHAR* buf)
{
    bool isDir =
        pEntry->GetRecordKind() == GenericEntry::kRecordKindVolumeDir ||
        pEntry->GetRecordKind() == GenericEntry::kRecordKindDirectory;

    if (pEntry->GetSourceFS() == DiskImg::kFormatMacHFS && isDir) {
        /* HFS directories don't have types; fake it */
        wcscpy(buf, L"    ");
    } else if (!(pEntry->GetFileType() >= 0 && pEntry->GetFileType() <= 0xff))
    {
        /* oversized type; assume it's HFS */
        MakeMacTypeString(pEntry->GetAuxType(), buf);
    } else {
        if (pEntry->GetRecordKind() == GenericEntry::kRecordKindDisk)
            wsprintf(buf, L"%I64dk", pEntry->GetUncompressedLen() / 1024);
        else
            wsprintf(buf, L"$%04lX", pEntry->GetAuxType());
    }
}

void ContentList::MakeRatioDisplayString(const GenericEntry* pEntry, WCHAR* buf,
    int* pPerc)
{
    LONGLONG totalLen, totalCompLen;
    totalLen = pEntry->GetUncompressedLen();
    totalCompLen = pEntry->GetCompressedLen();

    if ((!totalLen && totalCompLen) || (totalLen && !totalCompLen)) {
        wcscpy(buf, L"---");   /* weird */
        *pPerc = -1;
    } else if (totalLen < totalCompLen) {
        wcscpy(buf, L">100%"); /* compression failed? */
        *pPerc = 101;
    } else {
        *pPerc = ComputePercent(totalCompLen, totalLen);
        wsprintf(buf, L"%d%%", *pPerc);
    }
}

void ContentList::OnGetDispInfo(NMHDR* pnmh, LRESULT* pResult)
{
    static const WCHAR kAccessBits[] = L"dnb  iwr";
    LV_DISPINFO* plvdi = (LV_DISPINFO*) pnmh;
    CString str;

    if (fpArchive->GetReloadFlag()) {
        wcscpy(plvdi->item.pszText, L"");
        *pResult = 0;
        return;
    }

    //LOGI("OnGetDispInfo");

    if (plvdi->item.mask & LVIF_TEXT) {
        GenericEntry* pEntry = (GenericEntry*) plvdi->item.lParam;
        //GenericEntry* pEntry = fpArchive->GetEntry(plvdi->item.iItem);

        switch (plvdi->item.iSubItem) {
        case 0:     // pathname
            if ((int) wcslen(pEntry->GetDisplayName()) > plvdi->item.cchTextMax) {
                // looks like current limit is 264 chars, which we could hit
                wcsncpy(plvdi->item.pszText, pEntry->GetDisplayName(),
                    plvdi->item.cchTextMax);
                plvdi->item.pszText[plvdi->item.cchTextMax-1] = '\0';
            } else {
                wcscpy(plvdi->item.pszText, pEntry->GetDisplayName());
            }

#if 0   // no longer needed -- "display names" are converted to Unicode
            /*
             * Sanitize the string.  This is really only necessary for
             * HFS, which has 8-bit "Macintosh Roman" filenames.  The Win32
             * controls can deal with it, but it looks better if we massage
             * it a little.
             */
            {
                WCHAR* str = plvdi->item.pszText;

                while (*str != '\0') {
                    *str = DiskImg::MacToASCII((unsigned char) (*str));
                    str++;
                }
            }
#endif
            break;
        case 1:     // type
            MakeFileTypeDisplayString(pEntry, plvdi->item.pszText);
            break;
        case 2:     // auxtype
            MakeAuxTypeDisplayString(pEntry, plvdi->item.pszText);
            break;
        case 3:     // mod date
            {
                CString modDate;
                FormatDate(pEntry->GetModWhen(), &modDate);
                ::lstrcpy(plvdi->item.pszText, (LPCWSTR) modDate);
            }
            break;
        case 4:     // format
            ASSERT(pEntry->GetFormatStr() != NULL);
            wcscpy(plvdi->item.pszText, pEntry->GetFormatStr());
            break;
        case 5:     // size
            wsprintf(plvdi->item.pszText, L"%I64d", pEntry->GetUncompressedLen());
            break;
        case 6:     // ratio
            int crud;
            MakeRatioDisplayString(pEntry, plvdi->item.pszText, &crud);
            break;
        case 7:     // packed
            wsprintf(plvdi->item.pszText, L"%I64d", pEntry->GetCompressedLen());
            break;
        case 8:     // access
            WCHAR bitLabels[sizeof(kAccessBits)];
            int i, j, mask;

            for (i = 0, j = 0, mask = 0x80; i < 8; i++, mask >>= 1) {
                if (pEntry->GetAccess() & mask)
                    bitLabels[j++] = kAccessBits[i];
            }
            bitLabels[j] = '\0';
            ASSERT(j < sizeof(bitLabels));
            //::sprintf(plvdi->item.pszText, "0x%02x", pEntry->GetAccess());
            wcscpy(plvdi->item.pszText, bitLabels);
            break;
        case 9:     // NuRecordIdx [hidden]
            break;
        default:
            ASSERT(false);
            break;
        }
    }

    //if (plvdi->item.mask & LVIF_IMAGE) {
    //  LOGI("IMAGE req item=%d subitem=%d",
    //      plvdi->item.iItem, plvdi->item.iSubItem);
    //}

    *pResult = 0;
}

/*
 * Helper functions for sort routine.
 */
static inline int CompareUnsignedLong(uint32_t u1, uint32_t u2)
{
    if (u1 < u2)
        return -1;
    else if (u1 > u2)
        return 1;
    else
        return 0;
}
static inline int CompareLONGLONG(LONGLONG u1, LONGLONG u2)
{
    if (u1 < u2)
        return -1;
    else if (u1 > u2)
        return 1;
    else
        return 0;
}
static inline int CompareTime(time_t t1, time_t t2)
{
    if (t1 < t2)
        return -1;
    else if (t1 > t2)
        return 1;
    else
        return 0;
}

int CALLBACK ContentList::CompareFunc(LPARAM lParam1, LPARAM lParam2,
    LPARAM lParamSort)
{
    const GenericEntry* pEntry1 = (const GenericEntry*) lParam1;
    const GenericEntry* pEntry2 = (const GenericEntry*) lParam2;
    WCHAR tmpBuf1[16];       // needs >= 5 for file type compare, and
    WCHAR tmpBuf2[16];       // >= 7 for ratio string
    int result;

    /* for descending order, flip the parameters */
    if (lParamSort & kDescendingFlag) {
        const GenericEntry* tmp;
        lParamSort &= ~(kDescendingFlag);
        tmp = pEntry1;
        pEntry1 = pEntry2;
        pEntry2 = tmp;
    }

    switch (lParamSort) {
    case 0:     // pathname
        result = wcsicmp(pEntry1->GetDisplayName(), pEntry2->GetDisplayName());
        break;
    case 1:     // file type
        MakeFileTypeDisplayString(pEntry1, tmpBuf1);
        MakeFileTypeDisplayString(pEntry2, tmpBuf2);
        result = wcsicmp(tmpBuf1, tmpBuf2);
        if (result != 0)
            break;
        /* else fall through to case 2 */
    case 2:     // aux type
        if (pEntry1->GetRecordKind() == GenericEntry::kRecordKindDisk) {
            if (pEntry2->GetRecordKind() == GenericEntry::kRecordKindDisk) {
                result = pEntry1->GetAuxType() - pEntry2->GetAuxType();
            } else {
                result = -1;
            }
        } else if (pEntry2->GetRecordKind() == GenericEntry::kRecordKindDisk) {
            result = 1;
        } else {
            result = pEntry1->GetAuxType() - pEntry2->GetAuxType();
        }
        break;
    case 3:     // mod date
        result = CompareTime(pEntry1->GetModWhen(),
                    pEntry2->GetModWhen());
        break;
    case 4:     // format
        result = ::lstrcmp(pEntry1->GetFormatStr(), pEntry2->GetFormatStr());
        break;
    case 5:     // size
        result = CompareLONGLONG(pEntry1->GetUncompressedLen(),
                    pEntry2->GetUncompressedLen());
        break;
    case 6:     // ratio
        int perc1, perc2;
        MakeRatioDisplayString(pEntry1, tmpBuf1, &perc1);
        MakeRatioDisplayString(pEntry2, tmpBuf2, &perc2);
        result = perc1 - perc2;
        break;
    case 7:     // packed
        result = CompareLONGLONG(pEntry1->GetCompressedLen(),
                    pEntry2->GetCompressedLen());
        break;
    case 8:     // access
        result = CompareUnsignedLong(pEntry1->GetAccess(),
                    pEntry2->GetAccess());
        break;
    case kNumVisibleColumns:    // file-order sort
    default:
        result = pEntry1->GetIndex() - pEntry2->GetIndex();
        break;
    }

    return result;
}

int ContentList::LoadData(void)
{
    GenericEntry* pEntry;
    LV_ITEM lvi;
    int dirCount = 0;
    int idx = 0;

    DeleteAllItems();   // for Reload case

    pEntry = fpArchive->GetEntries();
    while (pEntry != NULL) {
        pEntry->SetIndex(idx);

        lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        lvi.iItem = idx++;
        lvi.iSubItem = 0;
        if (pEntry->GetDamaged())
            lvi.iImage = kListIconDamaged;
        else if (pEntry->GetSuspicious())
            lvi.iImage = kListIconSuspicious;
        else if (pEntry->GetHasNonEmptyComment())
            lvi.iImage = kListIconNonEmptyComment;
        else if (pEntry->GetHasComment())
            lvi.iImage = kListIconComment;
        else
            lvi.iImage = kListIconNone;
        lvi.pszText = LPSTR_TEXTCALLBACK;
        lvi.lParam = (LPARAM) pEntry;

        if (InsertItem(&lvi) == -1) {
            ASSERT(false);
            return -1;
        }

        pEntry = pEntry->GetNext();
    }

    LOGI("ContentList got %d entries (%d files + %d unseen directories)",
        idx, idx - dirCount, dirCount);
    return 0;
}

int ContentList::GetDefaultWidth(int col)
{
    int retval;

    switch (col) {
    case 0: // pathname
        retval = 200;
        break;
    case 1: // type (need "$XY" and long HFS types)
        retval = MaxVal(GetStringWidth(L"XXMMMM+"), GetStringWidth(L"XXType"));
        break;
    case 2: // auxtype (hex or long HFS type)
        retval = MaxVal(GetStringWidth(L"XX$CCCC"), GetStringWidth(L"XXAux"));
        break;
    case 3: // mod date
        retval = GetStringWidth(L"XX88-MMM-88 88:88");
        break;
    case 4: // format
        retval = GetStringWidth(L"XXUncompr");
        break;
    case 5: // uncompressed size
        retval = GetStringWidth(L"XX88888888");
        break;
    case 6: // ratio
        retval = MaxVal(GetStringWidth(L"XXRatio"), GetStringWidth(L"XX100%"));
        break;
    case 7: // packed
        retval = GetStringWidth(L"XX88888888");
        break;
    case 8: // access
        retval = MaxVal(GetStringWidth(L"XXAccess"), GetStringWidth(L"XXdnbiwr"));
        break;
    default:
        ASSERT(false);
        retval = 0;
    }

    return retval;
}

void ContentList::SetSortIcon(void)
{
    CHeaderCtrl* pHeader = GetHeaderCtrl();
    ASSERT(pHeader != NULL);
    HDITEM curItem;

    /* update all column headers */
    for (int i = 0; i < kNumVisibleColumns; i++) {
        curItem.mask = HDI_IMAGE | HDI_FORMAT;
        pHeader->GetItem(i, &curItem);

        if (fpLayout->GetSortColumn() != i) {
            curItem.fmt &= ~(HDF_IMAGE | HDF_BITMAP_ON_RIGHT);
        } else {
            //LOGI("  Sorting on %d", i);
            curItem.fmt |= HDF_IMAGE | HDF_BITMAP_ON_RIGHT;
            if (fpLayout->GetAscending())
                curItem.iImage = 0;
            else
                curItem.iImage = 1;
        }

        pHeader->SetItem(i, &curItem);
    }
}

void ContentList::OnDoubleClick(NMHDR*, LRESULT* pResult)
{
    /* test */
    DWORD dwPos = ::GetMessagePos();
    CPoint point ((int) LOWORD(dwPos), (int) HIWORD(dwPos));
    ScreenToClient(&point);

    int idx = HitTest(point);
    if (idx != -1) {
        CString str = GetItemText(idx, 0);
        LOGI("%ls was double-clicked", (LPCWSTR) str);
    }

    ((MainWindow*) ::AfxGetMainWnd())->HandleDoubleClick();
    *pResult = 0;
}

void ContentList::OnRightClick(NMHDR*, LRESULT* pResult)
{
    /*
     * -The first item in the menu performs the double-click action on the
     * -item clicked on.  The rest of the menu is simply a mirror of the items
     * -in the "Actions" menu.  To make this work, we let the main window handle
     * -everything, but save a copy of the index of the menu item that was
     * -clicked on.
     *
     * [We do this differently now?? ++ATM 20040722]
     */
    DWORD dwPos = ::GetMessagePos();
    CPoint point ((int) LOWORD(dwPos), (int) HIWORD(dwPos));
    ScreenToClient(&point);

#if 0
    int idx = HitTest(point);
    if (idx != -1) {
        CString str = GetItemText(idx, 0);
        LOGI("%ls was right-clicked", (LPCWSTR) str);

        //fRightClickItem = idx;
#else
    {
#endif

        CMenu menu;
        menu.LoadMenu(IDR_RIGHTCLICKMENU);
        CMenu* pContextMenu = menu.GetSubMenu(0);
        ClientToScreen(&point);
        pContextMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
            point.x, point.y, ::AfxGetMainWnd());
    }
    *pResult = 0;
}

void ContentList::SelectAll(void)
{
    int i;

    for (i = GetItemCount()-1; i >= 0; i--) {
        if (!SetItemState(i, LVIS_SELECTED, LVIS_SELECTED)) {
            LOGI("Glitch: SetItemState failed on %d", i);
        }
    }
}

void ContentList::InvertSelection(void)
{
    int i, oldState;

    for (i = GetItemCount()-1; i >= 0; i--) {
        oldState = GetItemState(i, LVIS_SELECTED);
        if (!SetItemState(i, oldState ? 0 : LVIS_SELECTED, LVIS_SELECTED)) {
            LOGI("Glitch: SetItemState failed on %d", i);
        }
    }
}

void ContentList::SelectSubdirContents(void)
{
    /*
     * We do the selection by prefix matching on the display name.  This means
     * we do one pass through the list for the contents of a subdir, including
     * all of its subdirs.  However, the subdirs we select as we're going will
     * be indistinguishable from subdirs selected by the user, which could
     * result in O(n^2) behavior.
     *
     * We mark the user's selection with LVIS_CUT, process them all, then go
     * back and clear all of the LVIS_CUT flags.  Of course, if they select
     * the entire archive, we're approach O(n^2) anyway.  If efficiency is a
     * problem we will need to sort the list, do some work, then sort it back
     * the way it was.
     *
     * This doesn't work for volume directories, because their display name
     * isn't quite right.  That's okay for now -- we document that we don't
     * allow deletion of the volume directory.  (We don't currently have a test
     * to see if a GenericEntry is a volume dir; might want to add one.)
     */
    POSITION posn;
    posn = GetFirstSelectedItemPosition();
    if (posn == NULL) {
        LOGI("SelectSubdirContents: nothing is selected");
        return;
    }
    /* mark all selected items with LVIS_CUT */
    while (posn != NULL) {
        int num = GetNextSelectedItem(/*ref*/ posn);
        SetItemState(num, LVIS_CUT, LVIS_CUT);
    }

    /* for each LVIS_CUT entry, select all prefix matches */
    CString prefix;
    for (int i = GetItemCount()-1; i >= 0; i--) {
        GenericEntry* pEntry = (GenericEntry*) GetItemData(i);
        bool origSel;

        origSel = GetItemState(i, LVIS_CUT) != 0;

        if (origSel &&
            (pEntry->GetRecordKind() == GenericEntry::kRecordKindDirectory ||
             pEntry->GetRecordKind() == GenericEntry::kRecordKindVolumeDir))
        {
            prefix = pEntry->GetDisplayName();
            prefix += pEntry->GetFssep();
            SelectSubdir(prefix);
        }

//      if (!SetItemState(i, oldState ? 0 : LVIS_SELECTED, LVIS_SELECTED)) {
//          LOGI("GLITCH: SetItemState failed on %d", i);
//      }
    }

    /* clear the LVIS_CUT flags */
    posn = GetFirstSelectedItemPosition();
    while (posn != NULL) {
        int num = GetNextSelectedItem(/*ref*/ posn);
        SetItemState(num, 0, LVIS_CUT);
    }
}

void ContentList::SelectSubdir(const WCHAR* displayPrefix)
{
    LOGI(" ContentList selecting all in '%ls'", displayPrefix);
    int len = wcslen(displayPrefix);

    for (int i = GetItemCount()-1; i >= 0; i--) {
        GenericEntry* pEntry = (GenericEntry*) GetItemData(i);

        if (wcsnicmp(displayPrefix, pEntry->GetDisplayName(), len) == 0)
            SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
    }
}

void ContentList::ClearSelection(void)
{
    for (int i = GetItemCount()-1; i >= 0; i--)
        SetItemState(i, 0, LVIS_SELECTED);
}

void ContentList::FindNext(const WCHAR* str, bool down, bool matchCase,
    bool wholeWord)
{
    POSITION posn;
    int i, num;
    bool found = false;

    LOGI("FindNext '%ls' d=%d c=%d w=%d", str, down, matchCase, wholeWord);

    posn = GetFirstSelectedItemPosition();
    num = GetNextSelectedItem(/*ref*/ posn);
    if (num < 0) {      // num will be -1 if nothing is selected
        if (down)
            num = -1;
        else
            num = GetItemCount();
    }

    LOGI("  starting search from entry %d", num);

    if (down) {
        for (i = num+1; i < GetItemCount(); i++) {
            found = CompareFindString(i, str, matchCase, wholeWord);
            if (found)
                break;
        }
        if (!found) {   // wrap
            for (i = 0; i <= num; i++) {
                found = CompareFindString(i, str, matchCase, wholeWord);
                if (found)
                    break;
            }
        }
    } else {
        for (i = num-1; i >= 0; i--) {
            found = CompareFindString(i, str, matchCase, wholeWord);
            if (found)
                break;
        }
        if (!found) {   // wrap
            for (i = GetItemCount()-1; i >= num; i--) {
                found = CompareFindString(i, str, matchCase, wholeWord);
                if (found)
                    break;
            }
        }
    }

    if (found) {
        LOGI("Found, i=%d", i);
        ClearSelection();
        SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
        EnsureVisible(i, false);
    } else {
        LOGI("Not found");
        MainWindow* pMain = (MainWindow*)::AfxGetMainWnd();
        pMain->FailureBeep();
    }
}

bool ContentList::CompareFindString(int num, const WCHAR* str, bool matchCase,
    bool wholeWord)
{
    GenericEntry* pEntry = (GenericEntry*) GetItemData(num);
    char fssep = pEntry->GetFssep();
    const WCHAR* (*pSubCompare)(const WCHAR* str, const WCHAR* subStr) = NULL;

    if (matchCase)
        pSubCompare = wcsstr;
    else
        pSubCompare = Stristr;

    if (wholeWord) {
        const WCHAR* src = pEntry->GetDisplayName();
        const WCHAR* start = src;
        size_t strLen = wcslen(str);

        /* scan forward, looking for a match that starts & ends on fssep */
        while (*start != '\0') {
            const WCHAR* match;

            match = (*pSubCompare)(start, str);

            if (match == NULL)
                break;
            if ((match == src || *(match-1) == fssep) &&
                (match[strLen] == '\0' || match[strLen] == fssep))
            {
                return true;
            }

            start++;
        }
    } else {
        if ((*pSubCompare)(pEntry->GetDisplayName(), str) != NULL)
            return true;
    }

    return false;
}
