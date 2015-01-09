/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Handle clipboard functions (copy, paste).  This is part of MainWindow,
 * split out into a separate file for clarity.
 */
#include "StdAfx.h"
#include "Main.h"
#include "PasteSpecialDialog.h"


static const WCHAR kClipboardFmtName[] = L"faddenSoft:CiderPress:v2";
const int kClipVersion = 2;     // should match "vN" in fmt name
const uint16_t kEntrySignature = 0x4350;

/*
 * Win98 is quietly dying on large (20MB-ish) copies.  Everything
 * runs along nicely and then, the next time we try to interact with
 * Windows, the entire system locks up and must be Ctrl-Alt-Deleted.
 *
 * In tests, it blew up on 16762187 but not 16682322, both of which
 * are shy of the 16MB mark (the former by about 15K).  This includes the
 * text version, which is potentially copied multiple times.  Windows doesn't
 * create the additional stuff (CF_OEMTEXT and CF_LOCALE; Win2K also creates
 * CF_UNICODETEXT) until after the clipboard is closed.  We can open & close
 * the clipboard to get an exact count, or just multiply by a small integer
 * to get a reasonable estimate (1x for alternate text, 2x for UNICODE).
 *
 * Microsoft Excel limits its clipboard to 4MB on small systems, and 8MB
 * on systems with at least 64MB of physical memory.  My guess is they
 * haven't really tried larger values.
 *
 * The description of GlobalAlloc suggests that it gets a little weird
 * when you go above 4MB, which makes me a little nervous about using
 * anything larger.  However, it seems to work very reliably until you
 * cross 16MB, at which point it seizes up 100% of the time.  It's possible
 * we're stomping on stuff and just getting lucky, but it's too-reliably
 * successful and too-reliably failing for me to believe that.
 */
const long kWin98ClipboardMax = 16 * 1024 * 1024;
const long kWin98NeutralZone = 512;     // extra padding
const int kClipTextMult = 4;    // CF_OEMTEXT, CF_LOCALE, CF_UNICODETEXT*2

/*
 * File collection header.
 */
typedef struct FileCollection {
    uint16_t    version;        // currently 1
    uint16_t    dataOffset;     // offset to start of data
    uint32_t    length;         // total length;
    uint32_t    count;          // #of entries
} FileCollection;

/* what kind of entry is this */
typedef enum EntryKind {
    kEntryKindUnknown = 0,
    kEntryKindFileDataFork,
    kEntryKindFileRsrcFork,
    kEntryKindFileBothForks,
    kEntryKindDirectory,
    kEntryKindDiskImage,
} EntryKind;

/*
 * One of these per entry in the collection.
 *
 * The next file starts at (start + dataOffset + dataLen + rsrcLen + cmmtLen).
 *
 * TODO: filename should be 8-bit from original, not 16-bit conversion
 */
typedef struct FileCollectionEntry {
    uint16_t    signature;      // let's be paranoid
    uint16_t    dataOffset;     // offset to start of data
    uint16_t    fileNameLen;    // len of filename, in bytes
    uint32_t    dataLen;        // len of data fork
    uint32_t    rsrcLen;        // len of rsrc fork
    uint32_t    cmmtLen;        // len of comments
    uint32_t    fileType;
    uint32_t    auxType;
    int64_t     createWhen;     // holds time_t
    int64_t     modWhen;        // holds time_t
    uint8_t     access;         // ProDOS access flags
    uint8_t     entryKind;      // GenericArchive::FileDetails::FileKind
    uint8_t     sourceFS;       // holds DiskImgLib::DiskImg::FSFormat
    uint8_t     fssep;          // filesystem separator char, e.g. ':'

    /* data comes next: null-terminated WCHAR filename, then data fork, then
       resource fork, then comment */
} FileCollectionEntry;


/*
 * ==========================================================================
 *      Copy
 * ==========================================================================
 */

void MainWindow::OnEditCopy(void)
{
    CString errStr, fileList;
    SelectionSet selSet;
    UINT myFormat;
    bool isOpen = false;
    HGLOBAL hGlobal;
    LPVOID pGlobal;
    uint8_t* buf = NULL;
    long bufLen = -1;

    /* associate a number with the format name */
    myFormat = RegisterClipboardFormat(kClipboardFmtName);
    if (myFormat == 0) {
        CheckedLoadString(&errStr, IDS_CLIPBOARD_REGFAILED);
        ShowFailureMsg(this, errStr, IDS_FAILED);
        goto bail;
    }
    LOGI("myFormat = %u", myFormat);

    /* open & empty the clipboard, even if we fail later */
    if (OpenClipboard() == false) {
        CheckedLoadString(&errStr, IDS_CLIPBOARD_OPENFAILED);
        ShowFailureMsg(this, errStr, IDS_FAILED);
        goto bail;
    }
    isOpen = true;
    EmptyClipboard();

    /*
     * Create a selection set with the entries.
     *
     * Strictly speaking we don't need the directories, since we recreate
     * them as needed.  However, storing them explicitly will allow us
     * to preserve empty subdirs.
     */
    selSet.CreateFromSelection(fpContentList,
        GenericEntry::kAnyThread | GenericEntry::kAllowDirectory);
    if (selSet.GetNumEntries() == 0) {
        CheckedLoadString(&errStr, IDS_CLIPBOARD_NOITEMS);
        MessageBox(errStr, L"No match", MB_OK | MB_ICONEXCLAMATION);
        goto bail;
    }

    /*
     * Make a big string with a file listing.
     */
    fileList = CreateFileList(&selSet);

    /*
     * Add the string to the clipboard.  The clipboard will own the memory we
     * allocate.
     */
    size_t neededLen = (fileList.GetLength() + 1) * sizeof(WCHAR);
    hGlobal = ::GlobalAlloc(GHND | GMEM_SHARE, neededLen);
    if (hGlobal == NULL) {
        LOGI("Failed allocating %d bytes", neededLen);
        CheckedLoadString(&errStr, IDS_CLIPBOARD_ALLOCFAILED);
        ShowFailureMsg(this, errStr, IDS_FAILED);
        goto bail;
    }
    LOGI("  Allocated %ld bytes for file list on clipboard", neededLen);
    pGlobal = ::GlobalLock(hGlobal);
    ASSERT(pGlobal != NULL);
    wcscpy((WCHAR*) pGlobal, fileList);
    ::GlobalUnlock(hGlobal);

    SetClipboardData(CF_UNICODETEXT, hGlobal);

    /*
     * Create a (potentially very large) buffer with the contents of the
     * files in it.  This may fail for any number of reasons.
     */
    hGlobal = CreateFileCollection(&selSet);
    if (hGlobal != NULL) {
        SetClipboardData(myFormat, hGlobal);
        // beep annoys me on copy
        //SuccessBeep();
    }

bail:
    CloseClipboard();
}

void MainWindow::OnUpdateEditCopy(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL &&
        fpContentList->GetSelectedCount() > 0);
}

CString MainWindow::CreateFileList(SelectionSet* pSelSet)
{
    SelectionEntry* pSelEntry;
    GenericEntry* pEntry;
    CString tmpStr, fullStr;
    WCHAR fileTypeBuf[ContentList::kFileTypeBufLen];
    WCHAR auxTypeBuf[ContentList::kAuxTypeBufLen];
    CString fileName, subVol, fileType, auxType, modDate, format, length;

    pSelEntry = pSelSet->IterNext();
    while (pSelEntry != NULL) {
        pEntry = pSelEntry->GetEntry();
        ASSERT(pEntry != NULL);

        fileName = DblDblQuote(pEntry->GetPathNameUNI());
        subVol = pEntry->GetSubVolName();
        ContentList::MakeFileTypeDisplayString(pEntry, fileTypeBuf);
        fileType = DblDblQuote(fileTypeBuf);  // Mac HFS types might have '"'?
        ContentList::MakeAuxTypeDisplayString(pEntry, auxTypeBuf);
        auxType = DblDblQuote(auxTypeBuf);
        FormatDate(pEntry->GetModWhen(), &modDate);
        format = pEntry->GetFormatStr();
        length.Format(L"%I64d", (LONGLONG) pEntry->GetUncompressedLen());

        tmpStr.Format(L"\"%ls\"\t%ls\t\"%ls\"\t\"%ls\"\t%ls\t%ls\t%ls\r\n",
            (LPCWSTR) fileName, (LPCWSTR) subVol, (LPCWSTR) fileType,
            (LPCWSTR) auxType, (LPCWSTR) modDate, (LPCWSTR) format,
            (LPCWSTR) length);
        fullStr += tmpStr;

        pSelEntry = pSelSet->IterNext();
    }

    return fullStr;
}

/*static*/ CString MainWindow::DblDblQuote(const WCHAR* str)
{
    CString result;
    WCHAR* buf;

    buf = result.GetBuffer(wcslen(str) * 2 +1);
    while (*str != '\0') {
        if (*str == '"') {
            *buf++ = *str;
            *buf++ = *str;
        } else {
            *buf++ = *str;
        }
        str++;
    }
    *buf = *str;

    result.ReleaseBuffer();

    return result;
}

long MainWindow::GetClipboardContentLen(void)
{
    long len = 0;
    UINT format = 0;
    HGLOBAL hGlobal;

    while ((format = EnumClipboardFormats(format)) != 0) {
        hGlobal = GetClipboardData(format);
        ASSERT(hGlobal != NULL);
        len += GlobalSize(hGlobal);
    }

    return len;
}

HGLOBAL MainWindow::CreateFileCollection(SelectionSet* pSelSet)
{
    SelectionEntry* pSelEntry;
    GenericEntry* pEntry;
    HGLOBAL hGlobal = NULL;
    HGLOBAL hResult = NULL;
    LPVOID pGlobal;
    size_t totalLength, numFiles;
    long priorLength;

    /* get len of text version(s), with kluge to avoid close & reopen */
    priorLength = GetClipboardContentLen() * kClipTextMult;
    /* add some padding -- textmult doesn't work for fixed-size CF_LOCALE */
    priorLength += kWin98NeutralZone;

    totalLength = sizeof(FileCollection);
    numFiles = 0;

    /*
     * Compute the amount of space required to hold it all.
     */
    pSelSet->IterReset();
    pSelEntry = pSelSet->IterNext();
    while (pSelEntry != NULL) {
        pEntry = pSelEntry->GetEntry();
        ASSERT(pEntry != NULL);

        //LOGI("+++ Examining '%s'", pEntry->GetDisplayName());

        if (pEntry->GetRecordKind() != GenericEntry::kRecordKindVolumeDir) {
            totalLength += sizeof(FileCollectionEntry);
            totalLength += (wcslen(pEntry->GetPathNameUNI()) +1) * sizeof(WCHAR);
            numFiles++;
            if (pEntry->GetRecordKind() != GenericEntry::kRecordKindDirectory) {
                totalLength += (long) pEntry->GetDataForkLen();
                totalLength += (long) pEntry->GetRsrcForkLen();
            }
        }

        if (totalLength < 0) {
            DebugBreak();
            LOGI("Overflow");    // pretty hard to do right now!
            return NULL;
        }

        pSelEntry = pSelSet->IterNext();
    }

#if 0
    {
        CString msg;
        msg.Format("totalLength is %ld+%ld = %ld",
            totalLength, priorLength, totalLength+priorLength);
        if (MessageBox(msg, NULL, MB_OKCANCEL) == IDCANCEL)
            goto bail;
    }
#endif

    LOGI("Total length required is %ld + %ld = %ld",
        totalLength, priorLength, totalLength+priorLength);
    if (IsWin9x() && totalLength+priorLength >= kWin98ClipboardMax) {
        CString errMsg;
        errMsg.Format(IDS_CLIPBOARD_WIN9XMAX,
            kWin98ClipboardMax / (1024*1024),
            ((float) (totalLength+priorLength)) / (1024.0*1024.0));
        ShowFailureMsg(this, errMsg, IDS_MB_APP_NAME);
        goto bail;
    }

    /*
     * Create a big buffer to hold it all.
     */
    hGlobal = ::GlobalAlloc(GHND | GMEM_SHARE, totalLength);
    if (hGlobal == NULL) {
        CString errMsg;
        errMsg.Format(L"ERROR: unable to allocate %ld bytes for copy",
            totalLength);
        LOGI("%ls", (LPCWSTR) errMsg);
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }
    pGlobal = ::GlobalLock(hGlobal);

    ASSERT(pGlobal != NULL);
    ASSERT(GlobalSize(hGlobal) >= (DWORD) totalLength);
    LOGI("hGlobal=0x%08lx pGlobal=0x%08lx size=%ld",
        (long) hGlobal, (long) pGlobal, GlobalSize(hGlobal));

    /*
     * Set up a progress dialog to track it.
     */
    ASSERT(fpActionProgress == NULL);
    fpActionProgress = new ActionProgressDialog;
    fpActionProgress->Create(ActionProgressDialog::kActionExtract, this);
    fpActionProgress->SetFileName(L"Clipboard");

    /*
     * Extract the data into the buffer.
     */
    long remainingLen;
    void* buf;

    remainingLen = totalLength - sizeof(FileCollection);
    buf = (uint8_t*) pGlobal + sizeof(FileCollection);
    pSelSet->IterReset();
    pSelEntry = pSelSet->IterNext();
    while (pSelEntry != NULL) {
        CString errStr;

        pEntry = pSelEntry->GetEntry();
        ASSERT(pEntry != NULL);

        fpActionProgress->SetArcName(pEntry->GetDisplayName());

        errStr = CopyToCollection(pEntry, &buf, &remainingLen);
        if (!errStr.IsEmpty()) {
            ShowFailureMsg(fpActionProgress, errStr, IDS_MB_APP_NAME);
            goto bail;
        }
        //LOGI("remainingLen now %ld", remainingLen);

        pSelEntry = pSelSet->IterNext();
    }

    ASSERT(remainingLen == 0);
    ASSERT(buf == (uint8_t*) pGlobal + totalLength);

    /*
     * Write the header.
     */
    FileCollection fileColl;
    fileColl.version = kClipVersion;
    fileColl.dataOffset = sizeof(FileCollection);
    fileColl.length = totalLength;
    fileColl.count = numFiles;
    memcpy(pGlobal, &fileColl, sizeof(fileColl));

    /*
     * Success!
     */
    ::GlobalUnlock(hGlobal);

    hResult = hGlobal;
    hGlobal = NULL;

bail:
    if (hGlobal != NULL) {
        ASSERT(hResult == NULL);
        ::GlobalUnlock(hGlobal);
        ::GlobalFree(hGlobal);
    }
    if (fpActionProgress != NULL) {
        fpActionProgress->Cleanup(this);
        fpActionProgress = NULL;
    }
    return hResult;
}

CString MainWindow::CopyToCollection(GenericEntry* pEntry, void** pBuf,
    long* pBufLen)
{
    FileCollectionEntry collEnt;
    CString errStr;
    uint8_t* buf = (uint8_t*) *pBuf;
    long remLen = *pBufLen;

    CheckedLoadString(&errStr, IDS_CLIPBOARD_WRITEFAILURE);

    if (pEntry->GetRecordKind() == GenericEntry::kRecordKindVolumeDir) {
        LOGI("Not copying volume dir to collection");
        return "";
    }

    if (remLen < sizeof(collEnt)) {
        ASSERT(false);
        return errStr;
    }

    GenericArchive::LocalFileDetails::FileKind entryKind;
    if (pEntry->GetRecordKind() == GenericEntry::kRecordKindDirectory)
        entryKind = GenericArchive::LocalFileDetails::kFileKindDirectory;
    else if (pEntry->GetHasDataFork() && pEntry->GetHasRsrcFork())
        entryKind = GenericArchive::LocalFileDetails::kFileKindBothForks;
    else if (pEntry->GetHasDataFork())
        entryKind = GenericArchive::LocalFileDetails::kFileKindDataFork;
    else if (pEntry->GetHasRsrcFork())
        entryKind = GenericArchive::LocalFileDetails::kFileKindRsrcFork;
    else if (pEntry->GetHasDiskImage())
        entryKind = GenericArchive::LocalFileDetails::kFileKindDiskImage;
    else {
        ASSERT(false);
        return errStr;
    }
    ASSERT((int) entryKind >= 0 && (int) entryKind <= 255);

    memset(&collEnt, 0x99, sizeof(collEnt));
    collEnt.signature = kEntrySignature;
    collEnt.dataOffset = sizeof(collEnt);
    collEnt.fileNameLen = (wcslen(pEntry->GetPathNameUNI()) +1) * sizeof(WCHAR);
    if (pEntry->GetRecordKind() == GenericEntry::kRecordKindDirectory) {
        collEnt.dataLen = collEnt.rsrcLen = collEnt.cmmtLen = 0;
    } else {
        collEnt.dataLen = (uint32_t) pEntry->GetDataForkLen();
        collEnt.rsrcLen = (uint32_t) pEntry->GetRsrcForkLen();
        collEnt.cmmtLen = 0;        // CMMT FIX -- length unknown??
    }
    collEnt.fileType = pEntry->GetFileType();
    collEnt.auxType = pEntry->GetAuxType();
    collEnt.createWhen = pEntry->GetCreateWhen();
    collEnt.modWhen = pEntry->GetModWhen();
    collEnt.access = (uint8_t) pEntry->GetAccess();
    collEnt.entryKind = (uint8_t) entryKind;
    collEnt.sourceFS = pEntry->GetSourceFS();
    collEnt.fssep = pEntry->GetFssep();

    /* verify there's enough space to hold everything */
    if ((uint32_t) remLen < collEnt.fileNameLen +
        collEnt.dataLen + collEnt.rsrcLen + collEnt.cmmtLen)
    {
        ASSERT(false);
        return errStr;
    }

    memcpy(buf, &collEnt, sizeof(collEnt));
    buf += sizeof(collEnt);
    remLen -= sizeof(collEnt);

    /* copy string with terminating null */
    memcpy(buf, pEntry->GetPathNameUNI(), collEnt.fileNameLen);
    buf += collEnt.fileNameLen;
    remLen -= collEnt.fileNameLen;

    /*
     * Extract data forks, resource forks, and disk images as appropriate.
     */
    char* bufCopy;
    long lenCopy;
    int result, which;

    if (pEntry->GetRecordKind() == GenericEntry::kRecordKindDirectory) {
        ASSERT(collEnt.dataLen == 0);
        ASSERT(collEnt.rsrcLen == 0);
        ASSERT(collEnt.cmmtLen == 0);
        ASSERT(!pEntry->GetHasRsrcFork());
    } else if (pEntry->GetHasDataFork() || pEntry->GetHasDiskImage()) {
        bufCopy = (char*) buf;
        lenCopy = remLen;
        if (pEntry->GetHasDiskImage())
            which = GenericEntry::kDiskImageThread;
        else
            which = GenericEntry::kDataThread;

        CString extractErrStr;
        result = pEntry->ExtractThreadToBuffer(which, &bufCopy, &lenCopy,
                    &extractErrStr);
        if (result == IDCANCEL) {
            CheckedLoadString(&errStr, IDS_CANCELLED);
            return errStr;
        } else if (result != IDOK) {
            LOGW("ExtractThreadToBuffer (data) failed: %ls",
                (LPCWSTR) extractErrStr);
            return errStr;
        }

        ASSERT(lenCopy == (long) collEnt.dataLen);
        buf += collEnt.dataLen;
        remLen -= collEnt.dataLen;
    } else {
        ASSERT(collEnt.dataLen == 0);
    }

    if (pEntry->GetHasRsrcFork()) {
        bufCopy = (char*) buf;
        lenCopy = remLen;
        which = GenericEntry::kRsrcThread;

        CString extractErrStr;
        result = pEntry->ExtractThreadToBuffer(which, &bufCopy, &lenCopy,
                    &extractErrStr);
        if (result == IDCANCEL) {
            CheckedLoadString(&errStr, IDS_CANCELLED);
            return errStr;
        } else if (result != IDOK) {
            LOGI("ExtractThreadToBuffer (rsrc) failed: %ls",
                (LPCWSTR) extractErrStr);
            return errStr;
        }

        ASSERT(lenCopy == (long) collEnt.rsrcLen);
        buf += collEnt.rsrcLen;
        remLen -= collEnt.rsrcLen;
    }

    if (pEntry->GetHasComment()) {
#if 0   // CMMT FIX
        bufCopy = (char*) buf;
        lenCopy = remLen;
        which = GenericEntry::kCommentThread;

        result = pEntry->ExtractThreadToBuffer(which, &bufCopy, &lenCopy);
        if (result == IDCANCEL) {
            errStr.LoadString(IDS_CANCELLED);
            return errStr;
        } else if (result != IDOK)
            return errStr;

        ASSERT(lenCopy == (long) collEnt.cmmtLen);
        buf += collEnt.cmmtLen;
        remLen -= collEnt.cmmtLen;
#else
        ASSERT(collEnt.cmmtLen == 0);
#endif
    }

    *pBuf = buf;
    *pBufLen = remLen;
    return "";
}


/*
 * ==========================================================================
 *      Paste
 * ==========================================================================
 */

void MainWindow::OnEditPaste(void)
{
    bool pasteJunkPaths = fPreferences.GetPrefBool(kPrPasteJunkPaths);

    DoPaste(pasteJunkPaths);
}

void MainWindow::OnUpdateEditPaste(CCmdUI* pCmdUI)
{
    bool dataAvailable = false;
    UINT myFormat;

    myFormat = RegisterClipboardFormat(kClipboardFmtName);
    if (myFormat != 0 && IsClipboardFormatAvailable(myFormat))
        dataAvailable = true;

    pCmdUI->Enable(fpContentList != NULL && !fpOpenArchive->IsReadOnly() &&
        dataAvailable);
}

void MainWindow::OnEditPasteSpecial(void)
{
    PasteSpecialDialog dlg;
    bool pasteJunkPaths = fPreferences.GetPrefBool(kPrPasteJunkPaths);

    // invert the meaning, so non-default mode is default in dialog
    if (pasteJunkPaths)
        dlg.fPasteHow = PasteSpecialDialog::kPastePaths;
    else
        dlg.fPasteHow = PasteSpecialDialog::kPasteNoPaths;
    if (dlg.DoModal() != IDOK)
        return;

    switch (dlg.fPasteHow) {
    case PasteSpecialDialog::kPastePaths:
        pasteJunkPaths = false;
        break;
    case PasteSpecialDialog::kPasteNoPaths:
        pasteJunkPaths = true;
        break;
    default:
        assert(false);
        break;
    }

    DoPaste(pasteJunkPaths);
}

void MainWindow::OnUpdateEditPasteSpecial(CCmdUI* pCmdUI)
{
    OnUpdateEditPaste(pCmdUI);
}

void MainWindow::DoPaste(bool pasteJunkPaths)
{
    CString errStr, buildStr;
    UINT format = 0;
    UINT myFormat;
    bool isOpen = false;

    if (fpContentList == NULL || fpOpenArchive->IsReadOnly()) {
        ASSERT(false);
        return;
    }

    myFormat = RegisterClipboardFormat(kClipboardFmtName);
    if (myFormat == 0) {
        CheckedLoadString(&errStr, IDS_CLIPBOARD_REGFAILED);
        ShowFailureMsg(this, errStr, IDS_FAILED);
        goto bail;
    }
    LOGI("myFormat = %u", myFormat);

    if (OpenClipboard() == false) {
        CheckedLoadString(&errStr, IDS_CLIPBOARD_OPENFAILED);
        ShowFailureMsg(this, errStr, IDS_FAILED);
        goto bail;;
    }
    isOpen = true;

    LOGI("Found %d clipboard formats", CountClipboardFormats());
    while ((format = EnumClipboardFormats(format)) != 0) {
        CString tmpStr;
        tmpStr.Format(L" %u", format);
        buildStr += tmpStr;
    }
    LOGI("  %ls", (LPCWSTR) buildStr);

#if 0
    if (IsClipboardFormatAvailable(CF_HDROP)) {
        errStr.LoadString(IDS_CLIPBOARD_NO_HDROP);
        ShowFailureMsg(this, errStr, IDS_FAILED);
        goto bail;
    }
#endif

    /* impossible unless OnUpdateEditPaste was bypassed */
    if (!IsClipboardFormatAvailable(myFormat)) {
        CheckedLoadString(&errStr, IDS_CLIPBOARD_NOTFOUND);
        ShowFailureMsg(this, errStr, IDS_FAILED);
        goto bail;
    }

    LOGI("+++ total data on clipboard: %ld bytes",
        GetClipboardContentLen());

    HGLOBAL hGlobal;
    LPVOID pGlobal;

    hGlobal = GetClipboardData(myFormat);
    if (hGlobal == NULL) {
        ASSERT(false);
        goto bail;
    }
    pGlobal = GlobalLock(hGlobal);
    ASSERT(pGlobal != NULL);
    errStr = ProcessClipboard(pGlobal, GlobalSize(hGlobal), pasteJunkPaths);
    fpContentList->Reload();

    if (!errStr.IsEmpty())
        ShowFailureMsg(this, errStr, IDS_FAILED);
    else
        SuccessBeep();

    GlobalUnlock(hGlobal);

bail:
    if (isOpen)
        CloseClipboard();
}

CString MainWindow::ProcessClipboard(const void* vbuf, long bufLen,
    bool pasteJunkPaths)
{
    FileCollection fileColl;
    CString errMsg, storagePrefix;
    const uint8_t* buf = (const uint8_t*) vbuf;
    DiskImgLib::A2File* pTargetSubdir = NULL;
    XferFileOptions xferOpts;
    bool xferPrepped = false;

    /* set a standard error message */
    CheckedLoadString(&errMsg, IDS_CLIPBOARD_READFAILURE);

    /*
     * Pull the header out.
     */
    if (bufLen < sizeof(fileColl)) {
        LOGW("Clipboard contents too small!");
        goto bail;
    }
    memcpy(&fileColl, buf, sizeof(fileColl));

    /*
     * Verify the length.  Win98 seems to like to round things up to 16-byte
     * boundaries, which screws up our "bufLen > 0" while condition below.
     */
    if ((long) fileColl.length > bufLen) {
        LOGW("GLITCH: stored len=%ld, clip len=%ld",
            fileColl.length, bufLen);
        goto bail;
    }
    if (bufLen > (long) fileColl.length) {
        /* trim off extra */
        LOGI("NOTE: Windows reports excess length (%ld vs %ld)",
            fileColl.length, bufLen);
        bufLen = fileColl.length;
    }

    buf += sizeof(fileColl);
    bufLen -= sizeof(fileColl);

    LOGI("FileCollection found: vers=%d off=%d len=%ld count=%ld",
        fileColl.version, fileColl.dataOffset, fileColl.length,
        fileColl.count);
    if (fileColl.count == 0) {
        /* nothing to do? */
        ASSERT(false);
        return errMsg;
    }

    /*
     * Figure out where we want to put the files.  For a disk archive
     * this can be complicated.
     *
     * The target DiskFS (which could be a sub-volume) gets tucked into
     * the xferOpts.
     */
    if (fpOpenArchive->GetArchiveKind() == GenericArchive::kArchiveDiskImage) {
        if (!ChooseAddTarget(&pTargetSubdir, &xferOpts.fpTargetFS))
            return L"";
    }
    fpOpenArchive->XferPrepare(&xferOpts);
    xferPrepped = true;

    if (pTargetSubdir != NULL) {
        storagePrefix = pTargetSubdir->GetPathName();
        LOGD("--- using storagePrefix '%ls'", (LPCWSTR) storagePrefix);
    }

    /*
     * Set up a progress dialog to track it.
     */
    ASSERT(fpActionProgress == NULL);
    fpActionProgress = new ActionProgressDialog;
    fpActionProgress->Create(ActionProgressDialog::kActionAdd, this);
    fpActionProgress->SetArcName(L"Clipboard data");

    /*
     * Loop over all files.
     */
    LOGI("+++ Starting paste, bufLen=%ld", bufLen);
    while (bufLen > 0) {
        FileCollectionEntry collEnt;
        CString fileName;
        CString processErrStr;

        /* read the entry info */
        if (bufLen < sizeof(collEnt)) {
            LOGW("GLITCH: bufLen=%ld, sizeof(collEnt)=%d",
                bufLen, sizeof(collEnt));
            ASSERT(false);
            goto bail;
        }
        memcpy(&collEnt, buf, sizeof(collEnt));
        if (collEnt.signature != kEntrySignature) {
            ASSERT(false);
            goto bail;
        }

        /* advance to the start of data */
        if (bufLen < collEnt.dataOffset) {
            ASSERT(false);
            goto bail;
        }
        buf += collEnt.dataOffset;
        bufLen -= collEnt.dataOffset;

        /* extract the filename */
        if (bufLen < collEnt.fileNameLen) {
            ASSERT(false);
            goto bail;
        }
        // TODO: consider moving filename as raw 8-bit data
        fileName = (const WCHAR*) buf;
        buf += collEnt.fileNameLen;
        bufLen -= collEnt.fileNameLen;

        LOGD("+++  pasting '%ls'", (LPCWSTR) fileName);

        /* strip the path (if requested) and prepend the storage prefix */
        ASSERT((fileName.GetLength() +1 ) * sizeof(WCHAR) == collEnt.fileNameLen);
        if (pasteJunkPaths && collEnt.fssep != '\0') {
            int idx;
            idx = fileName.ReverseFind(collEnt.fssep);
            if (idx >= 0)
                fileName = fileName.Right(fileName.GetLength() - idx -1);
        }
        if (!storagePrefix.IsEmpty()) {
            CString tmpStr, tmpFileName;
            tmpFileName = fileName;
            if (collEnt.fssep == '\0') {
                tmpFileName.Replace(':', '_');  // strip any ':'s in the name
                collEnt.fssep = ':';            // define an fssep
            }

            tmpStr = storagePrefix;

            /* storagePrefix fssep is always ':'; change it to match */
            if (collEnt.fssep != ':')
                tmpStr.Replace(':', collEnt.fssep);

            tmpStr += collEnt.fssep;
            tmpStr += tmpFileName;
            fileName = tmpStr;

        }
        fpActionProgress->SetFileName(fileName);

        /* make sure the data is there */
        if (bufLen < (long) (collEnt.dataLen + collEnt.rsrcLen + collEnt.cmmtLen))
        {
            ASSERT(false);
            goto bail;
        }

        /*
         * Process the entry.
         *
         * If the user hits "cancel" in the progress dialog we'll get thrown
         * back out.  For the time being I'm just treating it like any other
         * failure.
         */
        processErrStr = ProcessClipboardEntry(&collEnt, fileName, buf, bufLen);
        if (!processErrStr.IsEmpty()) {
            errMsg.Format(L"Unable to paste '%ls': %ls.",
                (LPCWSTR) fileName, (LPCWSTR) processErrStr);
            goto bail;
        }
        
        buf += collEnt.dataLen + collEnt.rsrcLen + collEnt.cmmtLen;
        bufLen -= collEnt.dataLen + collEnt.rsrcLen + collEnt.cmmtLen;
    }

    ASSERT(bufLen == 0);
    errMsg = "";

bail:
    if (xferPrepped) {
        if (errMsg.IsEmpty())
            fpOpenArchive->XferFinish(this);
        else
            fpOpenArchive->XferAbort(this);
    }
    if (fpActionProgress != NULL) {
        fpActionProgress->Cleanup(this);
        fpActionProgress = NULL;
    }
    return errMsg;
}

CString MainWindow::ProcessClipboardEntry(const FileCollectionEntry* pCollEnt,
    const WCHAR* pathName, const uint8_t* buf, long remLen)
{
    GenericArchive::LocalFileDetails::FileKind entryKind;
    GenericArchive::LocalFileDetails details;
    uint8_t* dataBuf = NULL;
    uint8_t* rsrcBuf = NULL;
    long dataLen, rsrcLen, cmmtLen;
    CString errMsg;

    entryKind = (GenericArchive::LocalFileDetails::FileKind) pCollEnt->entryKind;
    LOGD(" Processing '%ls' (%d)", pathName, entryKind);

    details.SetEntryKind(entryKind);
    details.SetLocalPathName(L"Clipboard");
    details.SetStrippedLocalPathName(pathName);
    details.SetFileSysFmt((DiskImg::FSFormat) pCollEnt->sourceFS);
    details.SetFssep(pCollEnt->fssep);
    details.SetAccess(pCollEnt->access);
    details.SetFileType(pCollEnt->fileType);
    details.SetExtraType(pCollEnt->auxType);
    NuDateTime ndt;
    GenericArchive::UNIXTimeToDateTime(&pCollEnt->createWhen, &ndt);
    details.SetCreateWhen(ndt);
    GenericArchive::UNIXTimeToDateTime(&pCollEnt->modWhen, &ndt);
    details.SetModWhen(ndt);
    time_t now = time(NULL);
    GenericArchive::UNIXTimeToDateTime(&now, &ndt);
    details.SetArchiveWhen(ndt);

    /*
     * Because of the way XferFile works, we need to make a copy of
     * the data.  (For NufxLib, it's going to gather up all of the
     * data and flush it all at once, so it needs to own the memory.)
     *
     * Ideally we'd use a different interface that didn't require a
     * data copy -- NufxLib can do it that way as well -- but it's
     * not worth maintaining two separate interfaces.
     *
     * This approach does allow the xfer code to handle DOS high-ASCII
     * text conversions in place, though.  If we didn't do it this way
     * we'd have to make a copy in the xfer code to avoid contaminating
     * the clipboard data.  That would be more efficient, but probably
     * a bit messier.
     *
     * The stuff below figures out which forks we're expected to have based
     * on the file type info.  This helps us distinguish between a file
     * with a zero-length fork and a file without that kind of fork.
     */
    bool hasData = false;
    bool hasRsrc = false;
    if (entryKind == GenericArchive::LocalFileDetails::kFileKindDataFork) {
        hasData = true;
        details.SetStorageType(kNuStorageSeedling);
    } else if (entryKind == GenericArchive::LocalFileDetails::kFileKindRsrcFork) {
        hasRsrc = true;
        details.SetStorageType(kNuStorageExtended);
    } else if (entryKind == GenericArchive::LocalFileDetails::kFileKindBothForks) {
        hasData = hasRsrc = true;
        details.SetStorageType(kNuStorageExtended);
    } else if (entryKind == GenericArchive::LocalFileDetails::kFileKindDiskImage) {
        hasData = true;
        details.SetStorageType(kNuStorageSeedling);
    } else if (entryKind == GenericArchive::LocalFileDetails::kFileKindDirectory) {
        details.SetStorageType(kNuStorageDirectory);
    } else {
        ASSERT(false);
        return L"Internal error.";
    }

    if (hasData) {
        if (pCollEnt->dataLen == 0) {
            dataBuf = new uint8_t[1];
            dataLen = 0;
        } else {
            dataLen = pCollEnt->dataLen;
            dataBuf = new uint8_t[dataLen];
            if (dataBuf == NULL)
                return L"memory allocation failed.";
            memcpy(dataBuf, buf, dataLen);
            buf += dataLen;
            remLen -= dataLen;
        }
    } else {
        ASSERT(dataBuf == NULL);
        dataLen = -1;
    }

    if (hasRsrc) {
        if (pCollEnt->rsrcLen == 0) {
            rsrcBuf = new uint8_t[1];
            rsrcLen = 0;
        } else {
            rsrcLen = pCollEnt->rsrcLen;
            rsrcBuf = new uint8_t[rsrcLen];
            if (rsrcBuf == NULL)
                return L"Memory allocation failed.";
            memcpy(rsrcBuf, buf, rsrcLen);
            buf += rsrcLen;
            remLen -= rsrcLen;
        }
    } else {
        ASSERT(rsrcBuf == NULL);
        rsrcLen = -1;
    }

    if (pCollEnt->cmmtLen > 0) {
        cmmtLen = pCollEnt->cmmtLen;
        /* CMMT FIX -- not supported by XferFile */
    }

    ASSERT(remLen >= 0);

    errMsg = fpOpenArchive->XferFile(&details, &dataBuf, dataLen,
                &rsrcBuf, rsrcLen);
    delete[] dataBuf;
    delete[] rsrcBuf;
    dataBuf = rsrcBuf = NULL;

    return errMsg;
}
