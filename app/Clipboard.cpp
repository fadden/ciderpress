/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Handle clipboard functions (copy, paste).
 */
#include "StdAfx.h"
#include "Main.h"
#include "PasteSpecialDialog.h"


static const char* kClipboardFmtName = "faddenSoft:CiderPress:v1";
const int kClipVersion = 1;     // should match "vN" in fmt name
const unsigned short kEntrySignature = 0x4350;

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
    unsigned short  version;        // currently 1
    unsigned short  dataOffset;     // offset to start of data
    unsigned long   length;         // total length;
    unsigned long   count;          // #of entries
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
 */
typedef struct FileCollectionEntry {
    unsigned short  signature;      // let's be paranoid
    unsigned short  dataOffset;     // offset to start of data
    unsigned short  fileNameLen;    // len of filename
    unsigned long   dataLen;        // len of data fork
    unsigned long   rsrcLen;        // len of rsrc fork
    unsigned long   cmmtLen;        // len of comments
    unsigned long   fileType;
    unsigned long   auxType;
    time_t          createWhen;
    time_t          modWhen;
    unsigned char   access;         // ProDOS access flags
    unsigned char   entryKind;      // GenericArchive::FileDetails::FileKind
    unsigned char   sourceFS;       // DiskImgLib::DiskImg::FSFormat
    unsigned char   fssep;          // filesystem separator char, e.g. ':'

    /* data comes next: filename, then data, then resource, then comment */
} FileCollectionEntry;


/*
 * ==========================================================================
 *      Copy
 * ==========================================================================
 */

/*
 * Copy data to the clipboard.
 */
void
MainWindow::OnEditCopy(void)
{
    CString errStr, fileList;
    SelectionSet selSet;
    UINT myFormat;
    bool isOpen = false;
    HGLOBAL hGlobal;
    LPVOID pGlobal;
    unsigned char* buf = nil;
    long bufLen = -1;

    /* associate a number with the format name */
    myFormat = RegisterClipboardFormat(kClipboardFmtName);
    if (myFormat == 0) {
        errStr.LoadString(IDS_CLIPBOARD_REGFAILED);
        ShowFailureMsg(this, errStr, IDS_FAILED);
        goto bail;
    }
    WMSG1("myFormat = %u\n", myFormat);

    /* open & empty the clipboard, even if we fail later */
    if (OpenClipboard() == false) {
        errStr.LoadString(IDS_CLIPBOARD_OPENFAILED);
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
        errStr.LoadString(IDS_CLIPBOARD_NOITEMS);
        MessageBox(errStr, "No match", MB_OK | MB_ICONEXCLAMATION);
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
    hGlobal = ::GlobalAlloc(GHND | GMEM_SHARE, fileList.GetLength() +1);
    if (hGlobal == nil) {
        WMSG1("Failed allocating %ld bytes\n", fileList.GetLength() +1);
        errStr.LoadString(IDS_CLIPBOARD_ALLOCFAILED);
        ShowFailureMsg(this, errStr, IDS_FAILED);
        goto bail;
    }
    WMSG1("  Allocated %ld bytes for file list on clipboard\n",
        fileList.GetLength() +1);
    pGlobal = ::GlobalLock(hGlobal);
    ASSERT(pGlobal != nil);
    strcpy((char*) pGlobal, fileList);
    ::GlobalUnlock(hGlobal);

    SetClipboardData(CF_TEXT, hGlobal);

    /*
     * Create a (potentially very large) buffer with the contents of the
     * files in it.  This may fail for any number of reasons.
     */
    hGlobal = CreateFileCollection(&selSet);
    if (hGlobal != nil) {
        SetClipboardData(myFormat, hGlobal);
        // beep annoys me on copy
        //SuccessBeep();
    }

bail:
    CloseClipboard();
}
void
MainWindow::OnUpdateEditCopy(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != nil &&
        fpContentList->GetSelectedCount() > 0);
}

/*
 * Create a list of selected files.
 *
 * The returned string uses tab-delineated fields with CSV-style quoting
 * around the filename (so that double quotes in the filename don't confuse
 * applications like MS Excel).
 */
CString
MainWindow::CreateFileList(SelectionSet* pSelSet)
{
    SelectionEntry* pSelEntry;
    GenericEntry* pEntry;
    CString tmpStr, fullStr;
    char fileTypeBuf[ContentList::kFileTypeBufLen];
    char auxTypeBuf[ContentList::kAuxTypeBufLen];
    CString fileName, subVol, fileType, auxType, modDate, format, length;

    pSelEntry = pSelSet->IterNext();
    while (pSelEntry != nil) {
        pEntry = pSelEntry->GetEntry();
        ASSERT(pEntry != nil);

        fileName = DblDblQuote(pEntry->GetPathName());
        subVol = pEntry->GetSubVolName();
        ContentList::MakeFileTypeDisplayString(pEntry, fileTypeBuf);
        fileType = DblDblQuote(fileTypeBuf);  // Mac HFS types might have '"'?
        ContentList::MakeAuxTypeDisplayString(pEntry, auxTypeBuf);
        auxType = DblDblQuote(auxTypeBuf);
        FormatDate(pEntry->GetModWhen(), &modDate);
        format = pEntry->GetFormatStr();
        length.Format("%I64d", (LONGLONG) pEntry->GetUncompressedLen());

        tmpStr.Format("\"%s\"\t%s\t\"%s\"\t\"%s\"\t%s\t%s\t%s\r\n",
            fileName, subVol, fileType, auxType, modDate, format, length);
        fullStr += tmpStr;

        pSelEntry = pSelSet->IterNext();
    }

    return fullStr;
}

/*
 * Double-up all double quotes.
 */
/*static*/ CString
MainWindow::DblDblQuote(const char* str)
{
    CString result;
    char* buf;

    buf = result.GetBuffer(strlen(str) * 2 +1);
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


/*
 * Compute the size of everything currently on the clipboard.
 */
long
MainWindow::GetClipboardContentLen(void)
{
    long len = 0;
    UINT format = 0;
    HGLOBAL hGlobal;

    while ((format = EnumClipboardFormats(format)) != 0) {
        hGlobal = GetClipboardData(format);
        ASSERT(hGlobal != nil);
        len += GlobalSize(hGlobal);
    }

    return len;
}

/*
 * Create the file collection.
 */
HGLOBAL
MainWindow::CreateFileCollection(SelectionSet* pSelSet)
{
    SelectionEntry* pSelEntry;
    GenericEntry* pEntry;
    HGLOBAL hGlobal = nil;
    HGLOBAL hResult = nil;
    LPVOID pGlobal;
    long totalLength, numFiles;
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
    while (pSelEntry != nil) {
        pEntry = pSelEntry->GetEntry();
        ASSERT(pEntry != nil);

        //WMSG1("+++ Examining '%s'\n", pEntry->GetDisplayName());

        if (pEntry->GetRecordKind() != GenericEntry::kRecordKindVolumeDir) {
            totalLength += sizeof(FileCollectionEntry);
            totalLength += strlen(pEntry->GetPathName()) +1;
            numFiles++;
            if (pEntry->GetRecordKind() != GenericEntry::kRecordKindDirectory) {
                totalLength += (long) pEntry->GetDataForkLen();
                totalLength += (long) pEntry->GetRsrcForkLen();
            }
        }

        if (totalLength < 0) {
            DebugBreak();
            WMSG0("Overflow\n");    // pretty hard to do right now!
            return nil;
        }

        pSelEntry = pSelSet->IterNext();
    }

#if 0
    {
        CString msg;
        msg.Format("totalLength is %ld+%ld = %ld",
            totalLength, priorLength, totalLength+priorLength);
        if (MessageBox(msg, nil, MB_OKCANCEL) == IDCANCEL)
            goto bail;
    }
#endif

    WMSG3("Total length required is %ld + %ld = %ld\n",
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
    if (hGlobal == nil) {
        CString errMsg;
        errMsg.Format("ERROR: unable to allocate %ld bytes for copy",
            totalLength);
        WMSG1("%s\n", (const char*) errMsg);
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }
    pGlobal = ::GlobalLock(hGlobal);

    ASSERT(pGlobal != nil);
    ASSERT(GlobalSize(hGlobal) >= (DWORD) totalLength);
    WMSG3("hGlobal=0x%08lx pGlobal=0x%08lx size=%ld\n",
        (long) hGlobal, (long) pGlobal, GlobalSize(hGlobal));

    /*
     * Set up a progress dialog to track it.
     */
    ASSERT(fpActionProgress == nil);
    fpActionProgress = new ActionProgressDialog;
    fpActionProgress->Create(ActionProgressDialog::kActionExtract, this);
    fpActionProgress->SetFileName("Clipboard");

    /*
     * Extract the data into the buffer.
     */
    long remainingLen;
    void* buf;

    remainingLen = totalLength - sizeof(FileCollection);
    buf = (unsigned char*) pGlobal + sizeof(FileCollection);
    pSelSet->IterReset();
    pSelEntry = pSelSet->IterNext();
    while (pSelEntry != nil) {
        CString errStr;

        pEntry = pSelEntry->GetEntry();
        ASSERT(pEntry != nil);

        fpActionProgress->SetArcName(pEntry->GetDisplayName());

        errStr = CopyToCollection(pEntry, &buf, &remainingLen);
        if (!errStr.IsEmpty()) {
            ShowFailureMsg(fpActionProgress, errStr, IDS_MB_APP_NAME);
            goto bail;
        }
        //WMSG1("remainingLen now %ld\n", remainingLen);

        pSelEntry = pSelSet->IterNext();
    }

    ASSERT(remainingLen == 0);
    ASSERT(buf == (unsigned char*) pGlobal + totalLength);

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
    hGlobal = nil;

bail:
    if (hGlobal != nil) {
        ASSERT(hResult == nil);
        ::GlobalUnlock(hGlobal);
        ::GlobalFree(hGlobal);
    }
    if (fpActionProgress != nil) {
        fpActionProgress->Cleanup(this);
        fpActionProgress = nil;
    }
    return hResult;
}

/*
 * Copy the contents of the file referred to by "pEntry" into the buffer
 * "*pBuf", which has "*pBufLen" bytes in it.
 *
 * The call fails if "*pBufLen" isn't large enough.
 *
 * Returns an empty string on success, or an error message on failure.
 * On success, "*pBuf" will be advanced past the data added, and "*pBufLen"
 * will be reduced by the amount of data copied into "buf".
 */
CString
MainWindow::CopyToCollection(GenericEntry* pEntry, void** pBuf, long* pBufLen)
{
    FileCollectionEntry collEnt;
    CString errStr, dummyStr;
    unsigned char* buf = (unsigned char*) *pBuf;
    long remLen = *pBufLen;

    errStr.LoadString(IDS_CLIPBOARD_WRITEFAILURE);

    if (pEntry->GetRecordKind() == GenericEntry::kRecordKindVolumeDir) {
        WMSG0("Not copying volume dir to collection\n");
        return "";
    }

    if (remLen < sizeof(collEnt)) {
        ASSERT(false);
        return errStr;
    }

    GenericArchive::FileDetails::FileKind entryKind;
    if (pEntry->GetRecordKind() == GenericEntry::kRecordKindDirectory)
        entryKind = GenericArchive::FileDetails::kFileKindDirectory;
    else if (pEntry->GetHasDataFork() && pEntry->GetHasRsrcFork())
        entryKind = GenericArchive::FileDetails::kFileKindBothForks;
    else if (pEntry->GetHasDataFork())
        entryKind = GenericArchive::FileDetails::kFileKindDataFork;
    else if (pEntry->GetHasRsrcFork())
        entryKind = GenericArchive::FileDetails::kFileKindRsrcFork;
    else if (pEntry->GetHasDiskImage())
        entryKind = GenericArchive::FileDetails::kFileKindDiskImage;
    else {
        ASSERT(false);
        return errStr;
    }
    ASSERT((int) entryKind >= 0 && (int) entryKind <= 255);

    memset(&collEnt, 0x99, sizeof(collEnt));
    collEnt.signature = kEntrySignature;
    collEnt.dataOffset = sizeof(collEnt);
    collEnt.fileNameLen = strlen(pEntry->GetPathName()) +1;
    if (pEntry->GetRecordKind() == GenericEntry::kRecordKindDirectory) {
        collEnt.dataLen = collEnt.rsrcLen = collEnt.cmmtLen = 0;
    } else {
        collEnt.dataLen = (unsigned long) pEntry->GetDataForkLen();
        collEnt.rsrcLen = (unsigned long) pEntry->GetRsrcForkLen();
        collEnt.cmmtLen = 0;        // CMMT FIX -- length unknown??
    }
    collEnt.fileType = pEntry->GetFileType();
    collEnt.auxType = pEntry->GetAuxType();
    collEnt.createWhen = pEntry->GetCreateWhen();
    collEnt.modWhen = pEntry->GetModWhen();
    collEnt.access = (unsigned char) pEntry->GetAccess();
    collEnt.entryKind = (unsigned char) entryKind;
    collEnt.sourceFS = pEntry->GetSourceFS();
    collEnt.fssep = pEntry->GetFssep();

    /* verify there's enough space to hold everything */
    if ((unsigned long) remLen < collEnt.fileNameLen +
        collEnt.dataLen + collEnt.rsrcLen + collEnt.cmmtLen)
    {
        ASSERT(false);
        return errStr;
    }

    memcpy(buf, &collEnt, sizeof(collEnt));
    buf += sizeof(collEnt);
    remLen -= sizeof(collEnt);

    /* copy string with terminating null */
    memcpy(buf, pEntry->GetPathName(), collEnt.fileNameLen);
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

        result = pEntry->ExtractThreadToBuffer(which, &bufCopy, &lenCopy,
                    &dummyStr);
        if (result == IDCANCEL) {
            errStr.LoadString(IDS_CANCELLED);
            return errStr;
        } else if (result != IDOK) {
            WMSG0("ExtractThreadToBuffer (data) failed\n");
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

        result = pEntry->ExtractThreadToBuffer(which, &bufCopy, &lenCopy,
                    &dummyStr);
        if (result == IDCANCEL) {
            errStr.LoadString(IDS_CANCELLED);
            return errStr;
        } else if (result != IDOK) {
            WMSG0("ExtractThreadToBuffer (rsrc) failed\n");
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

/*
 * Paste data from the clipboard, using the configured defaults.
 */
void
MainWindow::OnEditPaste(void)
{
    bool pasteJunkPaths = fPreferences.GetPrefBool(kPrPasteJunkPaths);

    DoPaste(pasteJunkPaths);
}
void
MainWindow::OnUpdateEditPaste(CCmdUI* pCmdUI)
{
    bool dataAvailable = false;
    UINT myFormat;

    myFormat = RegisterClipboardFormat(kClipboardFmtName);
    if (myFormat != 0 && IsClipboardFormatAvailable(myFormat))
        dataAvailable = true;

    pCmdUI->Enable(fpContentList != nil && !fpOpenArchive->IsReadOnly() &&
        dataAvailable);
}

/*
 * Paste data from the clipboard, giving the user the opportunity to select
 * how the files are handled.
 */
void
MainWindow::OnEditPasteSpecial(void)
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
void
MainWindow::OnUpdateEditPasteSpecial(CCmdUI* pCmdUI)
{
    OnUpdateEditPaste(pCmdUI);
}

/*
 * Do some prep work and then call ProcessClipboard to copy files in.
 */
void
MainWindow::DoPaste(bool pasteJunkPaths)
{
    CString errStr, buildStr;
    UINT format = 0;
    UINT myFormat;
    bool isOpen = false;

    if (fpContentList == nil || fpOpenArchive->IsReadOnly()) {
        ASSERT(false);
        return;
    }

    myFormat = RegisterClipboardFormat(kClipboardFmtName);
    if (myFormat == 0) {
        errStr.LoadString(IDS_CLIPBOARD_REGFAILED);
        ShowFailureMsg(this, errStr, IDS_FAILED);
        goto bail;
    }
    WMSG1("myFormat = %u\n", myFormat);

    if (OpenClipboard() == false) {
        errStr.LoadString(IDS_CLIPBOARD_OPENFAILED);
        ShowFailureMsg(this, errStr, IDS_FAILED);
        goto bail;;
    }
    isOpen = true;

    WMSG1("Found %d clipboard formats\n", CountClipboardFormats());
    while ((format = EnumClipboardFormats(format)) != 0) {
        CString tmpStr;
        tmpStr.Format(" %u", format);
        buildStr += tmpStr;
    }
    WMSG1("  %s\n", buildStr);

#if 0
    if (IsClipboardFormatAvailable(CF_HDROP)) {
        errStr.LoadString(IDS_CLIPBOARD_NO_HDROP);
        ShowFailureMsg(this, errStr, IDS_FAILED);
        goto bail;
    }
#endif

    /* impossible unless OnUpdateEditPaste was bypassed */
    if (!IsClipboardFormatAvailable(myFormat)) {
        errStr.LoadString(IDS_CLIPBOARD_NOTFOUND);
        ShowFailureMsg(this, errStr, IDS_FAILED);
        goto bail;
    }

    WMSG1("+++ total data on clipboard: %ld bytes\n",
        GetClipboardContentLen());

    HGLOBAL hGlobal;
    LPVOID pGlobal;

    hGlobal = GetClipboardData(myFormat);
    if (hGlobal == nil) {
        ASSERT(false);
        goto bail;
    }
    pGlobal = GlobalLock(hGlobal);
    ASSERT(pGlobal != nil);
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

/*
 * Process the data in the clipboard.
 *
 * Returns an empty string on success, or an error message on failure.
 */
CString
MainWindow::ProcessClipboard(const void* vbuf, long bufLen, bool pasteJunkPaths)
{
    FileCollection fileColl;
    CString errMsg, storagePrefix;
    const unsigned char* buf = (const unsigned char*) vbuf;
    DiskImgLib::A2File* pTargetSubdir = nil;
    XferFileOptions xferOpts;
    bool xferPrepped = false;

    /* set a standard error message */
    errMsg.LoadString(IDS_CLIPBOARD_READFAILURE);

    /*
     * Pull the header out.
     */
    if (bufLen < sizeof(fileColl)) {
        WMSG0("Clipboard contents too small!\n");
        goto bail;
    }
    memcpy(&fileColl, buf, sizeof(fileColl));

    /*
     * Verify the length.  Win98 seems to like to round things up to 16-byte
     * boundaries, which screws up our "bufLen > 0" while condition below.
     */
    if ((long) fileColl.length > bufLen) {
        WMSG2("GLITCH: stored len=%ld, clip len=%ld\n",
            fileColl.length, bufLen);
        goto bail;
    }
    if (bufLen > (long) fileColl.length) {
        /* trim off extra */
        WMSG2("NOTE: Windows reports excess length (%ld vs %ld)\n",
            fileColl.length, bufLen);
        bufLen = fileColl.length;
    }

    buf += sizeof(fileColl);
    bufLen -= sizeof(fileColl);

    WMSG4("FileCollection found: vers=%d off=%d len=%ld count=%ld\n",
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
            return "";
    }
    fpOpenArchive->XferPrepare(&xferOpts);
    xferPrepped = true;

    if (pTargetSubdir != nil) {
        storagePrefix = pTargetSubdir->GetPathName();
        WMSG1("--- using storagePrefix '%s'\n", (const char*) storagePrefix);
    }

    /*
     * Set up a progress dialog to track it.
     */
    ASSERT(fpActionProgress == nil);
    fpActionProgress = new ActionProgressDialog;
    fpActionProgress->Create(ActionProgressDialog::kActionAdd, this);
    fpActionProgress->SetArcName("Clipboard data");

    /*
     * Loop over all files.
     */
    WMSG1("+++ Starting paste, bufLen=%ld\n", bufLen);
    while (bufLen > 0) {
        FileCollectionEntry collEnt;
        CString fileName, processErrStr;

        /* read the entry info */
        if (bufLen < sizeof(collEnt)) {
            WMSG2("GLITCH: bufLen=%ld, sizeof(collEnt)=%d\n",
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
        fileName = buf;
        buf += collEnt.fileNameLen;
        bufLen -= collEnt.fileNameLen;

        //WMSG1("+++  pasting '%s'\n", fileName);

        /* strip the path (if requested) and prepend the storage prefix */
        ASSERT(fileName.GetLength() == collEnt.fileNameLen -1);
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
            errMsg.Format("Unable to paste '%s': %s.",
                (const char*) fileName, (const char*) processErrStr);
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
    if (fpActionProgress != nil) {
        fpActionProgress->Cleanup(this);
        fpActionProgress = nil;
    }
    return errMsg;
}

/*
 * Process a single clipboard entry.
 *
 * On entry, "buf" points to the start of the first chunk of data (either
 * data fork or resource fork).  If the file has empty forks or is a
 * subdirectory, then "buf" is actually pointing at the start of the
 * next entry.
 */
CString
MainWindow::ProcessClipboardEntry(const FileCollectionEntry* pCollEnt,
    const char* pathName, const unsigned char* buf, long remLen)
{
    GenericArchive::FileDetails::FileKind entryKind;
    GenericArchive::FileDetails details;
    unsigned char* dataBuf = nil;
    unsigned char* rsrcBuf = nil;
    long dataLen, rsrcLen, cmmtLen;
    CString errMsg;

    entryKind = (GenericArchive::FileDetails::FileKind) pCollEnt->entryKind;
    WMSG2(" Processing '%s' (%d)\n", pathName, entryKind);

    details.entryKind = entryKind;
    details.origName = "Clipboard";
    details.storageName = pathName;
    details.fileSysFmt = (DiskImg::FSFormat) pCollEnt->sourceFS;
    details.fileSysInfo = pCollEnt->fssep;
    details.access = pCollEnt->access;
    details.fileType = pCollEnt->fileType;
    details.extraType = pCollEnt->auxType;
    GenericArchive::UNIXTimeToDateTime(&pCollEnt->createWhen,
        &details.createWhen);
    GenericArchive::UNIXTimeToDateTime(&pCollEnt->modWhen,
        &details.modWhen);
    time_t now = time(nil);
    GenericArchive::UNIXTimeToDateTime(&now, &details.archiveWhen);

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
    if (details.entryKind == GenericArchive::FileDetails::kFileKindDataFork) {
        hasData = true;
        details.storageType = kNuStorageSeedling;
    } else if (details.entryKind == GenericArchive::FileDetails::kFileKindRsrcFork) {
        hasRsrc = true;
        details.storageType = kNuStorageExtended;
    } else if (details.entryKind == GenericArchive::FileDetails::kFileKindBothForks) {
        hasData = hasRsrc = true;
        details.storageType = kNuStorageExtended;
    } else if (details.entryKind == GenericArchive::FileDetails::kFileKindDiskImage) {
        hasData = true;
        details.storageType = kNuStorageSeedling;
    } else if (details.entryKind == GenericArchive::FileDetails::kFileKindDirectory) {
        details.storageType = kNuStorageDirectory;
    } else {
        ASSERT(false);
        return "Internal error.";
    }

    if (hasData) {
        if (pCollEnt->dataLen == 0) {
            dataBuf = new unsigned char[1];
            dataLen = 0;
        } else {
            dataLen = pCollEnt->dataLen;
            dataBuf = new unsigned char[dataLen];
            if (dataBuf == nil)
                return "memory allocation failed.";
            memcpy(dataBuf, buf, dataLen);
            buf += dataLen;
            remLen -= dataLen;
        }
    } else {
        ASSERT(dataBuf == nil);
        dataLen = -1;
    }

    if (hasRsrc) {
        if (pCollEnt->rsrcLen == 0) {
            rsrcBuf = new unsigned char[1];
            rsrcLen = 0;
        } else {
            rsrcLen = pCollEnt->rsrcLen;
            rsrcBuf = new unsigned char[rsrcLen];
            if (rsrcBuf == nil)
                return "Memory allocation failed.";
            memcpy(rsrcBuf, buf, rsrcLen);
            buf += rsrcLen;
            remLen -= rsrcLen;
        }
    } else {
        ASSERT(rsrcBuf == nil);
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
    dataBuf = rsrcBuf = nil;

    return errMsg;
}
