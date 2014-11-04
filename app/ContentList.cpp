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
    WMSG0("MOUSE WHEEL\n");
    return CWnd::OnMouseWheel(nFlags, zDelta, pt);
//  return TRUE;
}
#endif


/*
 * Put the window into "report" mode, and add a client edge since we're not
 * using one on the frame window.
 */
BOOL
ContentList::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CListCtrl::PreCreateWindow(cs))
        return FALSE;

    cs.style &= ~LVS_TYPEMASK;
    cs.style |= LVS_REPORT;
    cs.style |= LVS_SHOWSELALWAYS;
    cs.dwExStyle |= WS_EX_CLIENTEDGE;

    return TRUE;
}

/*
 * Auto-cleanup the object.
 */
void
ContentList::PostNcDestroy(void)
{
    WMSG0("ContentList PostNcDestroy\n");
    delete this;
}

static inline int
MaxVal(int a, int b)
{
    return a > b ? a : b;
}

/*
 * Create and populate list control.
 */
int
ContentList::OnCreate(LPCREATESTRUCT lpcs)
{
    CString colHdrs[kNumVisibleColumns] = {
        "Pathname", "Type", "Aux", "Mod Date",
        "Format", "Size", "Ratio", "Packed", "Access"
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
    if (pHeader == nil)
        WMSG0("GLITCH: couldn't get header ctrl\n");
    ASSERT(pHeader != NULL);
    pHeader->SetImageList(&fHdrImageList);

    /* load the data and sort it */
    if (LoadData() != 0) {
        MessageBox("Not all entries were loaded.", "Error",
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

/*
 * If we're being shut down, save off the column width info before the window
 * gets destroyed.
 */
void
ContentList::OnDestroy(void)
{
    WMSG0("ContentList OnDestroy\n");

    ExportColumnWidths();
    CListCtrl::OnDestroy();
}

/*
 * The system colors are changing; delete the image list and re-load it.
 */
void
ContentList::OnSysColorChange(void)
{
    fHdrImageList.DeleteImageList();
    LoadHeaderImages();
}

/*
 * They've clicked on a header.  Figure out what kind of sort order we want
 * to use.
 */
void
ContentList::OnColumnClick(NMHDR* pnmh, LRESULT* pResult)
{
    NM_LISTVIEW* pnmlv = (NM_LISTVIEW*) pnmh;

    WMSG0("OnColumnClick!!\n");

    if (fpLayout->GetSortColumn() == pnmlv->iSubItem)
        fpLayout->SetAscending(!fpLayout->GetAscending());
    else {
        fpLayout->SetSortColumn(pnmlv->iSubItem);
        fpLayout->SetAscending(true);
    }

    NewSortOrder();
    *pResult = 0;
}

/*
 * Copy the current column widths out to the Preferences object.
 */
void
ContentList::ExportColumnWidths(void)
{
    //WMSG0("ExportColumnWidths\n");
    for (int i = 0; i < kNumVisibleColumns; i++)
        fpLayout->SetColumnWidth(i, GetColumnWidth(i));
}

/*
 * Call this when the column widths are changed programmatically (e.g. by
 * the preferences page enabling or disabling columns).
 *
 * We want to set any defaulted entries to actual values so that, if the
 * font properties change, column A doesn't resize when column B is tweaked
 * in the Preferences dialog.  (If it's still set to "default", then when
 * we say "update all widths" the defaultedness will be re-evaluated.)
 */
void
ContentList::NewColumnWidths(void)
{
    for (int i = 0; i < kNumVisibleColumns; i++) {
        int width = fpLayout->GetColumnWidth(i);
        if (width == ColumnLayout::kWidthDefaulted) {
            width = GetDefaultWidth(i);
            WMSG2("Defaulting width %d to %d\n", i, width);
            fpLayout->SetColumnWidth(i, width);
        }
        SetColumnWidth(i, width);
    }
}

#if 0   // replaced by GenericArchive reload flag
/*
 * If we're in the middle of an update, invalidate the contents of the list
 * so that we don't try to redraw from underlying storage that is no longer
 * there.
 *
 * If we call DeleteAllItems the list will immediately blank itself.  This
 * rather sucks.  Instead, we just mark it as invalid and have the "virtual"
 * list goodies return empty strings.  If the window has to redraw it won't
 * do so properly, but most of the time it looks good and it beats flashing
 * blank or crashing.
 */
void
ContentList::Invalidate(void)
{
    fInvalid = true;
}
#endif

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
void
ContentList::Reload(bool saveSelection)
{
    WMSG0("Reloading ContentList\n");
    CWaitCursor waitc;

//  fInvalid = false;
    fpArchive->ClearReloadFlag();

    long* savedSel = nil;
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

    if (savedSel != nil) {
        /* restore the selection */
        RestoreSelection(savedSel, selCount);
        delete[] savedSel;
    }

    /* try to put us back in the same place */
    EnsureVisible(bottom, false);
    EnsureVisible(top, false);
}

#if 1
/*
 * Get the "selection serials" from the list of selected items.
 *
 * The caller is responsible for delete[]ing the return value.
 */
long*
ContentList::GetSelectionSerials(long* pSelCount)
{
    long* savedSel = nil;
    long maxCount;

    maxCount = GetSelectedCount();
    WMSG1("GetSelectionSerials (maxCount=%d)\n", maxCount);

    if (maxCount > 0) {
        savedSel = new long[maxCount];
        int idx = 0;

        POSITION posn;
        posn = GetFirstSelectedItemPosition();
        ASSERT(posn != nil);
        if (posn == nil)
            return nil;
        while (posn != nil) {
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

/*
 * Restore the selection from the "savedSel" list.
 */
void
ContentList::RestoreSelection(const long* savedSel, long selCount)
{
    WMSG1("RestoreSelection (selCount=%d)\n", selCount);
    if (savedSel == nil)
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
                    WMSG1("WHOA: unable to set selected on item=%d\n", i);
                }
                break;
            }
        }
    }
}
#endif


/*
 * Call this when the sort order changes.
 */
void
ContentList::NewSortOrder(void)
{
    CWaitCursor wait;       // automatically changes mouse to hourglass
    int column;

    column = fpLayout->GetSortColumn();
    if (!fpLayout->GetAscending())
        column |= kDescendingFlag;

    SetSortIcon();
    SortItems(CompareFunc, column);
}

/*
 * Get the file type display string.
 *
 * "buf" must be able to hold at least 4 characters plus the NUL (i.e. 5).
 * Use kFileTypeBufLen.
 */
/*static*/ void
ContentList::MakeFileTypeDisplayString(const GenericEntry* pEntry, char* buf)
{
    bool isDir =
        pEntry->GetRecordKind() == GenericEntry::kRecordKindVolumeDir ||
        pEntry->GetRecordKind() == GenericEntry::kRecordKindDirectory;

    if (pEntry->GetSourceFS() == DiskImg::kFormatMacHFS && isDir) {
        /* HFS directories don't have types; fake it */
        ::lstrcpy(buf, "DIR/");
    } else if (!(pEntry->GetFileType() >= 0 && pEntry->GetFileType() <= 0xff))
    {
        /* oversized type; assume it's HFS */
        char typeBuf[kFileTypeBufLen];
        MakeMacTypeString(pEntry->GetFileType(), typeBuf);

        switch (pEntry->GetRecordKind()) {
        case GenericEntry::kRecordKindFile:
            ::lstrcpy(buf, typeBuf);
            break;
        case GenericEntry::kRecordKindForkedFile:
            ::sprintf(buf, "%s+", typeBuf);
            break;
        case GenericEntry::kRecordKindUnknown:
            // shouldn't happen
            ::sprintf(buf, "%s-", typeBuf);
            break;
        case GenericEntry::kRecordKindVolumeDir:
        case GenericEntry::kRecordKindDirectory:
        case GenericEntry::kRecordKindDisk:
        default:
            ASSERT(FALSE);
            ::lstrcpy(buf, "!!!");
            break;
        }
    } else {
        /* typical ProDOS-style stuff */
        switch (pEntry->GetRecordKind()) {
        case GenericEntry::kRecordKindVolumeDir:
        case GenericEntry::kRecordKindDirectory:
            ::sprintf(buf, "%s/", pEntry->GetFileTypeString());
            break;
        case GenericEntry::kRecordKindFile:
            ::sprintf(buf, "%s", pEntry->GetFileTypeString());
            break;
        case GenericEntry::kRecordKindForkedFile:
            ::sprintf(buf, "%s+", pEntry->GetFileTypeString());
            break;
        case GenericEntry::kRecordKindDisk:
            ::lstrcpy(buf, "Disk");
            break;
        case GenericEntry::kRecordKindUnknown:
            // usually a GSHK-archived empty data file does this
            ::sprintf(buf, "%s-", pEntry->GetFileTypeString());
            break;
        default:
            ASSERT(FALSE);
            ::lstrcpy(buf, "!!!");
            break;
        }
    }
}

/*
 * Convert an HFS file/creator type into a string.
 *
 * "buf" must be able to hold at least 4 characters plus the NUL.  Use
 * kFileTypeBufLen.
 */
/*static*/ void
ContentList::MakeMacTypeString(unsigned long val, char* buf)
{
    /* expand longword with ASCII type bytes */
    buf[0] = (unsigned char) (val >> 24);
    buf[1] = (unsigned char) (val >> 16);
    buf[2] = (unsigned char) (val >> 8);
    buf[3] = (unsigned char) val;
    buf[4] = '\0';

    /* sanitize */
    while (*buf != '\0') {
        *buf = DiskImg::MacToASCII(*buf);
        buf++;
    }
}

/*
 * Get the aux type display string.
 *
 * "buf" must be able to hold at least 5 characters plus the NUL (i.e. 6).
 * Use kFileTypeBufLen.
 */
/*static*/ void
ContentList::MakeAuxTypeDisplayString(const GenericEntry* pEntry, char* buf)
{
    bool isDir =
        pEntry->GetRecordKind() == GenericEntry::kRecordKindVolumeDir ||
        pEntry->GetRecordKind() == GenericEntry::kRecordKindDirectory;

    if (pEntry->GetSourceFS() == DiskImg::kFormatMacHFS && isDir) {
        /* HFS directories don't have types; fake it */
        ::lstrcpy(buf, "    ");
    } else if (!(pEntry->GetFileType() >= 0 && pEntry->GetFileType() <= 0xff))
    {
        /* oversized type; assume it's HFS */
        MakeMacTypeString(pEntry->GetAuxType(), buf);
    } else {
        if (pEntry->GetRecordKind() == GenericEntry::kRecordKindDisk)
            ::sprintf(buf, "%dk", pEntry->GetUncompressedLen() / 1024);
        else
            ::sprintf(buf, "$%04lX", pEntry->GetAuxType());
    }
}


/*
 * Generate the funky ratio display string.  While we're at it, return a
 * numeric value that we can sort on.
 *
 * "buf" must be able to hold at least 6 chars plus the NULL.
 */
void
ContentList::MakeRatioDisplayString(const GenericEntry* pEntry, char* buf,
    int* pPerc)
{
    LONGLONG totalLen, totalCompLen;
    totalLen = pEntry->GetUncompressedLen();
    totalCompLen = pEntry->GetCompressedLen();

    if ((!totalLen && totalCompLen) || (totalLen && !totalCompLen)) {
        ::lstrcpy(buf, "---");   /* weird */
        *pPerc = -1;
    } else if (totalLen < totalCompLen) {
        ::lstrcpy(buf, ">100%"); /* compression failed? */
        *pPerc = 101;
    } else {
        *pPerc = ComputePercent(totalCompLen, totalLen);
        ::sprintf(buf, "%d%%", *pPerc);
    }
}


/*
 * Return the value for a particular row and column.
 *
 * This gets called *a lot* while the list is being drawn, scrolled, etc.
 * Don't do anything too expensive.
 */
void
ContentList::OnGetDispInfo(NMHDR* pnmh, LRESULT* pResult)
{
    //static const char kAccessBits[] = "DNB  IWR";
    static const char kAccessBits[] = "dnb  iwr";
    LV_DISPINFO* plvdi = (LV_DISPINFO*) pnmh;
    CString str;

    if (fpArchive->GetReloadFlag()) {
        ::lstrcpy(plvdi->item.pszText, "");
        *pResult = 0;
        return;
    }

    //WMSG0("OnGetDispInfo\n");

    if (plvdi->item.mask & LVIF_TEXT) {
        GenericEntry* pEntry = (GenericEntry*) plvdi->item.lParam;
        //GenericEntry* pEntry = fpArchive->GetEntry(plvdi->item.iItem);

        switch (plvdi->item.iSubItem) {
        case 0:     // pathname
            if ((int)strlen(pEntry->GetDisplayName()) > plvdi->item.cchTextMax) {
                // looks like current limit is 264 chars, which we could hit
                ::strncpy(plvdi->item.pszText, pEntry->GetDisplayName(),
                    plvdi->item.cchTextMax);
                plvdi->item.pszText[plvdi->item.cchTextMax-1] = '\0';
            } else {
                ::lstrcpy(plvdi->item.pszText, pEntry->GetDisplayName());
            }

            /*
             * Sanitize the string.  This is really only necessary for
             * HFS, which has 8-bit "Macintosh Roman" filenames.  The Win32
             * controls can deal with it, but it looks better if we massage
             * it a little.
             */
            {
                unsigned char* str = (unsigned char*) plvdi->item.pszText;

                while (*str != '\0') {
                    *str = DiskImg::MacToASCII(*str);
                    str++;
                }
            }
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
                ::lstrcpy(plvdi->item.pszText, (LPCTSTR) modDate);
            }
            break;
        case 4:     // format
            ASSERT(pEntry->GetFormatStr() != nil);
            ::lstrcpy(plvdi->item.pszText, pEntry->GetFormatStr());
            break;
        case 5:     // size
            ::sprintf(plvdi->item.pszText, "%ld", pEntry->GetUncompressedLen());
            break;
        case 6:     // ratio
            int crud;
            MakeRatioDisplayString(pEntry, plvdi->item.pszText, &crud);
            break;
        case 7:     // packed
            ::sprintf(plvdi->item.pszText, "%ld", pEntry->GetCompressedLen());
            break;
        case 8:     // access
            char bitLabels[sizeof(kAccessBits)];
            int i, j, mask;

            for (i = 0, j = 0, mask = 0x80; i < 8; i++, mask >>= 1) {
                if (pEntry->GetAccess() & mask)
                    bitLabels[j++] = kAccessBits[i];
            }
            bitLabels[j] = '\0';
            ASSERT(j < sizeof(bitLabels));
            //::sprintf(plvdi->item.pszText, "0x%02x", pEntry->GetAccess());
            ::lstrcpy(plvdi->item.pszText, bitLabels);
            break;
        case 9:     // NuRecordIdx [hidden]
            break;
        default:
            ASSERT(false);
            break;
        }
    }

    //if (plvdi->item.mask & LVIF_IMAGE) {
    //  WMSG2("IMAGE req item=%d subitem=%d\n",
    //      plvdi->item.iItem, plvdi->item.iSubItem);
    //}

    *pResult = 0;
}


/*
 * Helper functions for sort routine.
 */
static inline
CompareUnsignedLong(unsigned long u1, unsigned long u2)
{
    if (u1 < u2)
        return -1;
    else if (u1 > u2)
        return 1;
    else
        return 0;
}
static inline
CompareLONGLONG(LONGLONG u1, LONGLONG u2)
{
    if (u1 < u2)
        return -1;
    else if (u1 > u2)
        return 1;
    else
        return 0;
}

/*
 * Static comparison function for list sorting.
 */
int CALLBACK
ContentList::CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    const GenericEntry* pEntry1 = (const GenericEntry*) lParam1;
    const GenericEntry* pEntry2 = (const GenericEntry*) lParam2;
    char tmpBuf1[16];       // needs >= 5 for file type compare, and
    char tmpBuf2[16];       // >= 7 for ratio string
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
        result = ::stricmp(pEntry1->GetDisplayName(), pEntry2->GetDisplayName());
        break;
    case 1:     // file type
        MakeFileTypeDisplayString(pEntry1, tmpBuf1);
        MakeFileTypeDisplayString(pEntry2, tmpBuf2);
        result = ::stricmp(tmpBuf1, tmpBuf2);
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
        result = CompareUnsignedLong(pEntry1->GetModWhen(),
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

/*
 * Fill the columns with data from the archive entries.  We use a "virtual"
 * list control to avoid storing everything multiple times.  However, we
 * still create one item per entry so that the list control will do most
 * of the sorting for us (otherwise we have to do the sorting ourselves).
 *
 * Someday we should probably move to a wholly virtual list view.
 */
int
ContentList::LoadData(void)
{
    GenericEntry* pEntry;
    LV_ITEM lvi;
    int dirCount = 0;
    int idx = 0;

    DeleteAllItems();   // for Reload case

    pEntry = fpArchive->GetEntries();
    while (pEntry != nil) {
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

    WMSG3("ContentList got %d entries (%d files + %d unseen directories)\n",
        idx, idx - dirCount, dirCount);
    return 0;
}


/*
 * Return the default width for the specified column.
 */
int
ContentList::GetDefaultWidth(int col)
{
    int retval;

    switch (col) {
    case 0: // pathname
        retval = 200;
        break;
    case 1: // type (need "$XY" and long HFS types)
        //retval = MaxVal(GetStringWidth("XXMMM+"), GetStringWidth("XXType"));
        retval = MaxVal(GetStringWidth("XXMMMM+"), GetStringWidth("XXType"));
        break;
    case 2: // auxtype (hex or long HFS type)
        //retval = MaxVal(GetStringWidth("XX$8888"), GetStringWidth("XXAux"));
        retval = MaxVal(GetStringWidth("XX$CCCC"), GetStringWidth("XXAux"));
        break;
    case 3: // mod date
        retval = GetStringWidth("XX88-MMM-88 88:88");
        break;
    case 4: // format
        retval = GetStringWidth("XXUncompr");
        break;
    case 5: // uncompressed size
        retval = GetStringWidth("XX88888888");
        break;
    case 6: // ratio
        retval = MaxVal(GetStringWidth("XXRatio"), GetStringWidth("XX100%"));
        break;
    case 7: // packed
        retval = GetStringWidth("XX88888888");
        break;
    case 8: // access
        retval = MaxVal(GetStringWidth("XXAccess"), GetStringWidth("XXdnbiwr"));
        break;
    default:
        ASSERT(false);
        retval = 0;
    }

    return retval;
}


/*
 * Set the up/down sorting arrow as appropriate.
 */
void
ContentList::SetSortIcon(void)
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
            //WMSG1("  Sorting on %d\n", i);
            curItem.fmt |= HDF_IMAGE | HDF_BITMAP_ON_RIGHT;
            if (fpLayout->GetAscending())
                curItem.iImage = 0;
            else
                curItem.iImage = 1;
        }

        pHeader->SetItem(i, &curItem);
    }
}


/*
 * Handle a double-click on an item.
 *
 * The double-click should single-select the item, so we can throw it
 * straight into the viewer.  However, there are some uses for bulk
 * double-clicking.
 */
void
ContentList::OnDoubleClick(NMHDR*, LRESULT* pResult)
{
    /* test */
    DWORD dwPos = ::GetMessagePos();
    CPoint point ((int) LOWORD(dwPos), (int) HIWORD(dwPos));
    ScreenToClient(&point);

    int idx = HitTest(point);
    if (idx != -1) {
        CString str = GetItemText(idx, 0);
        WMSG1("%s was double-clicked\n", str);
    }

    ((MainWindow*) ::AfxGetMainWnd())->HandleDoubleClick();
    *pResult = 0;
}

/*
 * Handle a right-click on an item.
 *
 * -The first item in the menu performs the double-click action on the
 * -item clicked on.  The rest of the menu is simply a mirror of the items
 * -in the "Actions" menu.  To make this work, we let the main window handle
 * -everything, but save a copy of the index of the menu item that was
 * -clicked on.
 *
 * [We do this differently now?? ++ATM 20040722]
 */
void
ContentList::OnRightClick(NMHDR*, LRESULT* pResult)
{
    DWORD dwPos = ::GetMessagePos();
    CPoint point ((int) LOWORD(dwPos), (int) HIWORD(dwPos));
    ScreenToClient(&point);

#if 0
    int idx = HitTest(point);
    if (idx != -1) {
        CString str = GetItemText(idx, 0);
        //TRACE1("%s was right-clicked\n", str);
        WMSG1("%s was right-clicked\n", str);

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

/*
 * Mark everything as selected.
 */
void
ContentList::SelectAll(void)
{
    int i;

    for (i = GetItemCount()-1; i >= 0; i--) {
        if (!SetItemState(i, LVIS_SELECTED, LVIS_SELECTED)) {
            WMSG1("Glitch: SetItemState failed on %d\n", i);
        }
    }
}

/*
 * Toggle the "selected" state flag.
 */
void
ContentList::InvertSelection(void)
{
    int i, oldState;

    for (i = GetItemCount()-1; i >= 0; i--) {
        oldState = GetItemState(i, LVIS_SELECTED);
        if (!SetItemState(i, oldState ? 0 : LVIS_SELECTED, LVIS_SELECTED)) {
            WMSG1("Glitch: SetItemState failed on %d\n", i);
        }
    }
}

/*
 * Select the contents of any selected subdirs.
 *
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
void
ContentList::SelectSubdirContents(void)
{
    POSITION posn;
    posn = GetFirstSelectedItemPosition();
    if (posn == nil) {
        WMSG0("SelectSubdirContents: nothing is selected\n");
        return;
    }
    /* mark all selected items with LVIS_CUT */
    while (posn != nil) {
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
//          WMSG1("GLITCH: SetItemState failed on %d\n", i);
//      }
    }

    /* clear the LVIS_CUT flags */
    posn = GetFirstSelectedItemPosition();
    while (posn != nil) {
        int num = GetNextSelectedItem(/*ref*/ posn);
        SetItemState(num, 0, LVIS_CUT);
    }
}

/*
 * Select every entry whose display name has "displayPrefix" as a prefix.
 */
void
ContentList::SelectSubdir(const char* displayPrefix)
{
    WMSG1(" ContentList selecting all in '%s'\n", displayPrefix);
    int len = strlen(displayPrefix);

    for (int i = GetItemCount()-1; i >= 0; i--) {
        GenericEntry* pEntry = (GenericEntry*) GetItemData(i);

        if (strncasecmp(displayPrefix, pEntry->GetDisplayName(), len) == 0)
            SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
    }
}

/*
 * Mark all items as unselected.
 */
void
ContentList::ClearSelection(void)
{
    for (int i = GetItemCount()-1; i >= 0; i--)
        SetItemState(i, 0, LVIS_SELECTED);
}

/*
 * Find the next matching entry.  We start after the first selected item.
 * If we find a matching entry, we clear the current selection and select it.
 */
void
ContentList::FindNext(const char* str, bool down, bool matchCase,
    bool wholeWord)
{
    POSITION posn;
    int i, num;
    bool found = false;

    WMSG4("FindNext '%s' d=%d c=%d w=%d\n", str, down, matchCase, wholeWord);

    posn = GetFirstSelectedItemPosition();
    num = GetNextSelectedItem(/*ref*/ posn);
    if (num < 0) {      // num will be -1 if nothing is selected
        if (down)
            num = -1;
        else
            num = GetItemCount();
    }

    WMSG1("  starting search from entry %d\n", num);

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
        WMSG1("Found, i=%d\n", i);
        ClearSelection();
        SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
        EnsureVisible(i, false);
    } else {
        WMSG0("Not found\n");
        MainWindow* pMain = (MainWindow*)::AfxGetMainWnd();
        pMain->FailureBeep();
    }
}

/*
 * Compare "str" against the contents of entry "num".
 */
bool
ContentList::CompareFindString(int num, const char* str, bool matchCase,
    bool wholeWord)
{
    GenericEntry* pEntry = (GenericEntry*) GetItemData(num);
    char fssep = pEntry->GetFssep();
    char* (*pSubCompare)(const char* str, const char* subStr) = nil;

    if (matchCase)
        pSubCompare = strstr;
    else
        pSubCompare = stristr;

    if (wholeWord) {
        const char* src = pEntry->GetDisplayName();
        const char* start = src;
        int strLen = strlen(str);

        /* scan forward, looking for a match that starts & ends on fssep */
        while (*start != '\0') {
            const char* match;

            match = (*pSubCompare)(start, str);

            if (match == nil)
                break;
            if ((match == src || *(match-1) == fssep) &&
                (match[strLen] == '\0' || match[strLen] == fssep))
            {
                return true;
            }

            start++;
        }
    } else {
        if ((*pSubCompare)(pEntry->GetDisplayName(), str) != nil)
            return true;
    }

    return false;
}
