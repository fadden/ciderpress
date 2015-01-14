/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Application UI classes.
 */
#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "ContentList.h"
#include "GenericArchive.h"
#include "PrefsDialog.h"
#include "ActionProgressDialog.h"
#include "ProgressCounterDialog.h"
#include "AddFilesDialog.h"
#include "ExtractOptionsDialog.h"
#include "ConvFileOptionsDialog.h"
#include "DiskConvertDialog.h"
#include "FileNameConv.h"
//#include "ProgressCancelDialog.h"

/* user-defined window messages */
#define WMU_LATE_INIT           (WM_USER+0)
#define WMU_START               (WM_USER+1)     // used by ActionProgressDialog

// The filter index is saved in the registry, so if you reorder this list
// you will briefly annoy existing users.
enum FilterIndex {
    kFilterIndexFIRST = 1,          // first index, must be non-Generic
    kFilterIndexNuFX = 1,
    kFilterIndexBinaryII = 2,
    kFilterIndexACU = 3,
    kFilterIndexAppleSingle = 4,
    kFilterIndexDiskImage = 5,
    kFilterIndexLASTNG = 5,         // last non-Generic index

    kFilterIndexGeneric = 6,        // *.* filter used
    kFilterIndexMAX = 6             // highest valid number
};

struct FileCollectionEntry;     // fwd

/*
 * The main UI window.
 */
class MainWindow : public CFrameWnd
{
public:
    MainWindow(void);
    ~MainWindow(void);

    /*
     * Override the pre-create function to tweak the window style.
     */
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;

    /*
     * Override GetClientRect so we can factor in the status and tool bars.
     *
     * (The method in question isn't declared virtual, so we're not actually
     * overriding it.)
     */
    void GetClientRect(LPRECT lpRect) const;

    // get a pointer to the preferences
    const Preferences* GetPreferences(void) const { return &fPreferences; }
    Preferences* GetPreferencesWr(void) { return &fPreferences; }
    // apply an update from the Preferences pages
    void ApplyNow(PrefsSheet*);

    // get the text of the next file in the selection list
    int GetPrevFileText(ReformatHolder* pHolder, CString* pTitle);
    int GetNextFileText(ReformatHolder* pHolder, CString* pTitle);

    // update the progress meter
    void SetProgressBegin(void);
    int SetProgressUpdate(int percent, const WCHAR* oldName,
        const WCHAR* newName);
    void SetProgressEnd(void);

    /*
     * Set a number in the "progress counter".  Useful for loading large archives
     * where we're not sure how much stuff is left, so showing a percentage is
     * hard.
     *
     * Pass in -1 to erase the counter.
     *
     * Returns "true" if we'd like things to continue.
     */
    bool SetProgressCounter(const WCHAR* fmt, long val);

    /*
     * Handle a double-click in the content view.
     *
     * Individual items get special treatment, multiple items just get handed off
     * to the file viewer.
     */
    void HandleDoubleClick(void);

    // do some idle processing
    void DoIdle(void);

    /*
     * Come up with a title to put at the top of a printout.  This is essentially
     * the same as the window title, but without some flags (e.g. "read-only").
     */
    CString GetPrintTitle(void);

    // raise flag to abort the current print job
    void SetAbortPrinting(bool val) { fAbortPrinting = val; }
    bool GetAbortPrinting(void) const { return fAbortPrinting; }

    /*
     * Printer abort procedure; allows us to abort a print job.  The DC
     * SetAbortProc() function calls here periodically.  The return value from
     * this function determine whether or not printing halts.
     *
     * This checks a global "print cancel" variable, which is set by our print
     * cancel button dialog.
     *
     * If this returns TRUE, printing continues; FALSE, and printing aborts.
     */
    static BOOL CALLBACK PrintAbortProc(HDC hDC, int nCode);

    bool            fAbortPrinting;
    // track printer choice
    HANDLE          fhDevMode;
    HANDLE          fhDevNames;

    // set flag to abort current operation
    //void SetAbortOperation(bool val) { fAbortOperation = val; }
    //bool          fAbortOperation;

    /*
     * Go to sleep for a little bit, waking up 100x per second to check
     * the idle loop.  Used for debugging.
     */
    void EventPause(int duration);

    ContentList* GetContentList(void) const { return fpContentList; }

    void SetActionProgressDialog(ActionProgressDialog* pActionProgress) {
        fpActionProgress = pActionProgress;
    }
    void SetProgressCounterDialog(ProgressCounterDialog* pProgressCounter) {
        fpProgressCounter = pProgressCounter;
    }
    GenericArchive* GetOpenArchive(void) const { return fpOpenArchive; }

    /*
     * Extract every part of the file into "ReformatHolder".  Does not try to
     * reformat anything, just extracts the parts.
     *
     * Returns IDOK on success, IDCANCEL if the user cancelled, or -1 on error.
     * On error, the reformatted text buffer gets the error message.
     */
    int GetFileParts(const GenericEntry* pEntry,
        ReformatHolder** ppHolder) const;

    /*
     * Allow events to flow through the message queue whenever the
     * progress meter gets updated.  This will allow us to redraw with
     * reasonable frequency.
     *
     * Calling this can result in other code being called, such as Windows
     * message handlers, which can lead to reentrancy problems.  Make sure
     * you're adequately semaphored before calling here.
     *
     * Returns TRUE if all is well, FALSE if we're trying to quit.
     */
    BOOL PeekAndPump();

    /*
     * After successful completion of a command, make a happy noise (but only
     * if we're configured to do so).
     */
    void SuccessBeep(void);

    /*
     * If something fails, make noise if we're configured for loudness.
     */
    void FailureBeep(void);

    /*
     * Remove a file.  Returns a helpful error string on failure.
     *
     * The absence of the file is not considered an error.
     */
    CString RemoveFile(const WCHAR* fileName);

    /*
     * Figure out where they want to add files.
     *
     * If the volume directory of a disk is chosen, *ppTargetSubdir will
     * be set to NULL.
     */
    bool ChooseAddTarget(DiskImgLib::A2File** ppTargetSubdir,
        DiskImgLib::DiskFS** ppDiskFS);

    /*
     * Put up the ImageFormatDialog and apply changes to "pImg".
     *
     * "*pDisplayFormat" gets the result of user changes to the display format.
     * If "pDisplayFormat" is NULL, the "query image format" feature will be
     * disabled.
     *
     * Returns IDCANCEL if the user cancelled out of the dialog, IDOK otherwise.
     * On error, "*pErrMsg" will be non-empty.
     */
    int TryDiskImgOverride(DiskImg* pImg, const WCHAR* fileSource,
        DiskImg::FSFormat defaultFormat, int* pDisplayFormat,
        bool allowUnknown, CString* pErrMsg);

    /*
     * Do a block copy or track copy from one disk image to another.
     *
     * If "bulk" is set, warning dialogs are suppressed.  If "partial" is set,
     * copies between volumes of different sizes are allowed.
     */
    DIError CopyDiskImage(DiskImg* pDstImg, DiskImg* pSrcImg, bool bulk,
        bool partial, ProgressCancelDialog* pPCDialog);

    // Determine whether path matches the pathname of the currently open archive.
    bool IsOpenPathName(const WCHAR* path);

    // raise a flag to cause a full reload of the open file
    void SetReopenFlag(void) { fNeedReopen = true; }

    /*
     * Configure a ReformatHolder based on the current preferences.
     */
    static void ConfigureReformatFromPreferences(ReformatHolder* pReformat);

    /*
     * Convert a DiskImg format spec into a ReformatHolder SourceFormat.
     */
    static ReformatHolder::SourceFormat ReformatterSourceFormat(DiskImg::FSFormat format);

    /*
     * Saves a buffer of data as a file in a disk image or file archive.
     * Utility function used by cassette import.  
     *
     * May modify contents of *pDetails.
     *
     * On failure, returns with an error message in errMsg.
     */
    static bool SaveToArchive(GenericArchive::LocalFileDetails* pDetails,
        const uint8_t* dataBuf, long dataLen,
        const uint8_t* rsrcBuf, long rsrcLen,
        CString* pErrMsg, CWnd* pDialog);

    static const WCHAR kOpenNuFX[];
    static const WCHAR kOpenBinaryII[];
    static const WCHAR kOpenACU[];
    static const WCHAR kOpenAppleSingle[];
    static const WCHAR kOpenDiskImage[];
    static const WCHAR kOpenAll[];
    static const WCHAR kOpenEnd[];

private:
    static const WCHAR kModeNuFX[];
    static const WCHAR kModeBinaryII[];
    static const WCHAR kModeACU[];
    static const WCHAR kModeAppleSingle[];
    static const WCHAR kModeDiskImage[];

    // Command handlers
    afx_msg int OnCreate(LPCREATESTRUCT lpcs);
    afx_msg LONG OnLateInit(UINT, LONG);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* pMMI);
    afx_msg void OnPaint(void);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo);
    afx_msg BOOL OnQueryEndSession(void);
    afx_msg void OnEndSession(BOOL bEnding);
    afx_msg LRESULT OnFindDialogMessage(WPARAM wParam, LPARAM lParam);
    afx_msg void OnFileNewArchive(void);
    afx_msg void OnFileOpen(void);
    afx_msg void OnFileOpenVolume(void);
    afx_msg void OnUpdateFileOpenVolume(CCmdUI* pCmdUI);
    afx_msg void OnFileReopen(void);
    afx_msg void OnUpdateFileReopen(CCmdUI* pCmdUI);
    afx_msg void OnFileSave(void);
    afx_msg void OnUpdateFileSave(CCmdUI* pCmdUI);
    afx_msg void OnFileClose(void);
    afx_msg void OnUpdateFileClose(CCmdUI* pCmdUI);
    afx_msg void OnFileArchiveInfo(void);
    afx_msg void OnUpdateFileArchiveInfo(CCmdUI* pCmdUI);
    afx_msg void OnFilePrint(void);
    afx_msg void OnUpdateFilePrint(CCmdUI* pCmdUI);
    afx_msg void OnFileExit(void);

    /*
     * Copy data to the clipboard.
     */
    afx_msg void OnEditCopy(void);
    afx_msg void OnUpdateEditCopy(CCmdUI* pCmdUI);

    /*
     * Paste data from the clipboard, using the configured defaults.
     */
    afx_msg void OnEditPaste(void);
    afx_msg void OnUpdateEditPaste(CCmdUI* pCmdUI);

    /*
     * Paste data from the clipboard, giving the user the opportunity to select
     * how the files are handled.
     */
    afx_msg void OnEditPasteSpecial(void);
    afx_msg void OnUpdateEditPasteSpecial(CCmdUI* pCmdUI);

    afx_msg void OnEditFind(void);
    afx_msg void OnUpdateEditFind(CCmdUI* pCmdUI);
    afx_msg void OnEditSelectAll(void);
    afx_msg void OnUpdateEditSelectAll(CCmdUI* pCmdUI);
    afx_msg void OnEditInvertSelection(void);
    afx_msg void OnUpdateEditInvertSelection(CCmdUI* pCmdUI);
    afx_msg void OnEditPreferences(void);
    afx_msg void OnEditSort(UINT id);
    afx_msg void OnUpdateEditSort(CCmdUI* pCmdUI);
    /*
     * View a file stored in the archive.
     *
     * Control bounces back through Get*FileText() to get the actual
     * data to view.
     */
    afx_msg void OnActionsView(void);
    afx_msg void OnUpdateActionsView(CCmdUI* pCmdUI);

    /*
     * View a file stored in the archive.
     *
     * Control bounces back through Get*FileText() to get the actual
     * data to view.
     */
    afx_msg void OnActionsOpenAsDisk(void);
    afx_msg void OnUpdateActionsOpenAsDisk(CCmdUI* pCmdUI);

    /*
     * Add files to an archive.
     */
    afx_msg void OnActionsAddFiles(void);
    afx_msg void OnUpdateActionsAddFiles(CCmdUI* pCmdUI);

    /*
     * Add a disk to an archive.  Not all archive formats support disk images.
     *
     * We open a single disk archive file as a DiskImg, get the format
     * figured out, then write it block-by-block into a file chosen by the user.
     * Standard open/save dialogs work fine here.
     */
    afx_msg void OnActionsAddDisks(void);
    afx_msg void OnUpdateActionsAddDisks(CCmdUI* pCmdUI);

    /*
     * Create a subdirectory inside another subdirectory (or volume directory).
     *
     * Simply asserting that an existing subdir be selected in the list does
     * away with all sorts of testing.  Creating subdirs on DOS disks and NuFX
     * archives is impossible because neither has subdirs.  Nested volumes are
     * selected for us by the user.
     */
    afx_msg void OnActionsCreateSubdir(void);
    afx_msg void OnUpdateActionsCreateSubdir(CCmdUI* pCmdUI);

    /*
     * Extract files.
     */
    afx_msg void OnActionsExtract(void);
    afx_msg void OnUpdateActionsExtract(CCmdUI* pCmdUI);

    /*
     * Test files.
     */
    afx_msg void OnActionsTest(void);
    afx_msg void OnUpdateActionsTest(CCmdUI* pCmdUI);

    /*
     * Delete archive entries.
     */
    afx_msg void OnActionsDelete(void);
    afx_msg void OnUpdateActionsDelete(CCmdUI* pCmdUI);

    /*
     * Rename archive entries.  Depending on the structure of the underlying
     * archive, we may only allow the user to alter the filename component.
     * Anything else would constitute moving the file around in the filesystem.
     */
    afx_msg void OnActionsRename(void);
    afx_msg void OnUpdateActionsRename(CCmdUI* pCmdUI);

    /*
     * Edit a comment, creating it if necessary.
     */
    afx_msg void OnActionsEditComment(void);
    afx_msg void OnUpdateActionsEditComment(CCmdUI* pCmdUI);

    /*
     * Edit file properties.
     *
     * This causes a reload of the list, which isn't really necessary.  We
     * do need to re-evaluate the sort order if one of the fields they modified
     * is the current sort key, but it would be nice if we could at least retain
     * the selection.  Since we're not reloading the GenericArchive, we *can*
     * remember the selection.
     */
    afx_msg void OnActionsEditProps(void);
    afx_msg void OnUpdateActionsEditProps(CCmdUI* pCmdUI);

    /*
     * Change a volume name or volume number.
     */
    afx_msg void OnActionsRenameVolume(void);
    afx_msg void OnUpdateActionsRenameVolume(CCmdUI* pCmdUI);

    /*
     * Recompress files.
     */
    afx_msg void OnActionsRecompress(void);
    afx_msg void OnUpdateActionsRecompress(CCmdUI* pCmdUI);

    /*
     * Select files to convert.
     */
    afx_msg void OnActionsConvDisk(void);
    afx_msg void OnUpdateActionsConvDisk(CCmdUI* pCmdUI);

    /*
     * Select files to convert.
     */
    afx_msg void OnActionsConvFile(void);
    afx_msg void OnUpdateActionsConvFile(CCmdUI* pCmdUI);

    /*
     * Convert BAS, INT, or BIN to a cassette-audio WAV file.
     * (not implemented)
     */
    afx_msg void OnActionsConvToWav(void);
    afx_msg void OnUpdateActionsConvToWav(CCmdUI* pCmdUI);

    /*
     * Convert a WAV file with a digitized Apple II cassette tape into an
     * Apple II file, and add it to the current disk.
     */
    afx_msg void OnActionsConvFromWav(void);
    afx_msg void OnUpdateActionsConvFromWav(CCmdUI* pCmdUI);

    /*
     * Import an Applesoft BASIC program from a text file.
     *
     * We currently allow the user to select a single file for import.  Someday
     * we may want to allow multi-file import.
     */
    afx_msg void OnActionsImportBAS(void);
    afx_msg void OnUpdateActionsImportBAS(CCmdUI* pCmdUI);

    // edit a disk
    afx_msg void OnToolsDiskEdit(void);
    // convert a disk image from one format to another
    afx_msg void OnToolsDiskConv(void);
    // bulk disk conversion
    afx_msg void OnToolsBulkDiskConv(void);
    // merge two SST images into a single NIB image
    afx_msg void OnToolsSSTMerge(void);
    afx_msg void OnToolsVolumeCopierVolume(void);
    afx_msg void OnToolsVolumeCopierFile(void);
    // scan and report on the end-of-line markers found in a file
    afx_msg void OnToolsEOLScanner(void);
    // edit the properties (but not the disk image inside) a .2MG disk image
    afx_msg void OnToolsTwoImgProps(void);
    // create a new disk image
    afx_msg void OnToolsDiskImageCreator(void);
    afx_msg void OnHelpContents(void);
    afx_msg void OnHelpWebSite(void);
    afx_msg void OnHelpOrdering(void);
    afx_msg void OnHelpAbout(void);
    afx_msg void OnRtClkDefault(void);

    // Handle command-line arguments.
    void ProcessCommandLine(void);

    void ResizeClientArea(void);

    /*
     * Draw what looks like an empty client area.
     */
    void DrawEmptyClientArea(CDC* pDC, const CRect& clientRect);

    /*
     * Extract a record to the temp folder and open it with a new instance of
     * CiderPress.  We might want to extract disk images as 2MG files to take
     * the mystery out of opening them, but since they're coming out of a
     * ShrinkIt archive they're pretty un-mysterious anyway.
     *
     * We tell the new instance to open it read-only, and flag it for
     * deletion on exit.
     *
     * Returns 0 on success, nonzero error status on failure.
     */
    int TmpExtractAndOpen(GenericEntry* pEntry, int threadKind,
        const WCHAR* modeStr);

    /*
     * Extract a record to the temp folder and open it with an external viewer.
     * The file must be created with the correct extension so ShellExecute
     * does the right thing.
     *
     * The files will be added to the "delete on exit" list, so that they will
     * be cleaned up when CiderPress exits (assuming the external viewer no longer
     * has them open).
     *
     * The GetTempFileName function creates a uniquely-named temp file.  We
     * create a file that has that name plus an extension.  To ensure that we
     * don't try to use the same temp filename twice, we have to hold off on
     * deleting the unused .tmp files until we're ready to delete the
     * corresponding .gif (or whatever) files.  Thus, each invocation of this
     * function creates two files and two entries in the delete-on-exit set.
     *
     * Returns 0 on success, nonzero error status on failure.
     */
    int TmpExtractForExternal(GenericEntry* pEntry);

    void DoOpenArchive(const WCHAR* pathName, const WCHAR* ext,
        FilterIndex filterIndex, bool readOnly);

    GenericArchive* CreateArchiveInstance(FilterIndex filterIndex) const;

    /*
     * Load an archive, using the appropriate GenericArchive subclass.
     *
     * "filename" is the full path to the file, "extension" is the
     * filetype component of the name (without the leading '.'), "filterIndex"
     * is the offset into the set of filename filters used in the standard
     * file dialog, and "readOnly" reflects the state of the stdfile dialog
     * checkbox.
     *
     * Returns 0 on success, nonzero on failure.
     */
    int LoadArchive(const WCHAR* filename, const WCHAR* extension,
            FilterIndex filterIndex, bool readOnly);

    /*
     * Open a raw disk volume.  Useful for ProDOS-formatted 1.44MB floppy disks
     * and CFFA flash cards.
     *
     * Assume it's a disk image -- it'd be a weird place for a ShrinkIt archive.
     * CFFA cards can actually hold multiple volumes, but that's all taken care
     * of inside the diskimg DLL.
     *
     * Returns 0 on success, nonzero on failure.
     */
    int DoOpenVolume(CString drive, bool readOnly);

    /*
     * Switch the content list to a new archive, closing the previous if one
     * was already open.
     */
    void SwitchContentList(GenericArchive* pOpenArchive);

    /*
     * Close the existing archive file, but don't try to shut down the child
     * windows.  This should only be used from the destructor.
     */
    void CloseArchiveWOControls(void);

    /*
     * Close the existing archive file, and throw out the control we're
     * using to display it.
     */
    void CloseArchive(void);

    /*
     * Set the title bar on the main window.
     *
     * "pathname" is often different from pOpenArchive->GetPathName(), especially
     * when we were launched from another instance of CiderPress and handed a
     * temp file whose name we're trying to conceal.
     */
    void SetCPTitle(const WCHAR* pathname, GenericArchive* pArchive);

    /*
     * Set the title bar to something boring when nothing is open.
     */
    void SetCPTitle(void);

    /*
     * Get the one selected item from the current display.  Primarily useful
     * for the double-click handler, but also used for "action" menu items
     * that insist on operating on a single menu item (edit prefs, create subdir).
     *
     * Returns NULL if the item couldn't be found or if more than one item was
     * selected.
     */
    GenericEntry* GetSelectedItem(ContentList* pContentList);

    /*
     * Handle a request to view stuff.
     *
     * If "query" pref is set, we ask the user to confirm some choices.  If
     * not, we just go with the defaults.
     *
     * We include "damaged" files so that we can show the user a nice message
     * about how the file is damaged.
     */
    void HandleView(void);

    void DeleteFileOnExit(const WCHAR* name);

    /*
     * Close and re-open the current archive.
     */
    void ReopenArchive(void);

    /*
     * ===== Actions.cpp =====
     */

    /*
     * Load the requested part.
     */
    void GetFilePart(const GenericEntry* pEntry, int whichThread,
        ReformatHolder* pHolder) const;

    /*
     * Handle a bulk extraction.
     */
    void DoBulkExtract(SelectionSet* pSelSet,
        const ExtractOptionsDialog* pExtOpts);

    /*
     * Extract a single entry.
     *
     * If pHolder is non-NULL, it holds the data from the file, and can be
     * used for formatted or non-formatted output.  If it's NULL, we need to
     * extract the data ourselves.
     *
     * Returns true on success, false on failure.
     */
    bool ExtractEntry(GenericEntry* pEntry, int thread,
        ReformatHolder* pHolder, const ExtractOptionsDialog* pExtOpts,
        bool* pOverwriteExisting, bool* pOvwrForAll);

    /*
     * Open an output file.
     *
     * "outputPath" holds the name of the file to create.  "origPath" is the
     * name as it was stored in the archive.  "pOverwriteExisting" tells us
     * if we should just go ahead and overwrite the existing file, while
     * "pOvwrForAll" tells us if a "To All" button was hit previously.
     *
     * If the file exists, *pOverwriteExisting is false, and *pOvwrForAll
     * is false, then we will put up the "do you want to overwrite?" dialog.
     * One possible outcome of the dialog is renaming the output path.
     *
     * On success, *pFp will be non-NULL, and IDOK will be returned.  On
     * failure, IDCANCEL will be returned.  The values in *pOverwriteExisting
     * and *pOvwrForAll may be updated, and *pOutputPath will change if
     * the user chose to rename the file.
     */
    int OpenOutputFile(CString* pOutputPath, const PathProposal& pathProp,
        time_t arcFileModWhen, bool* pOverwriteExisting, bool* pOvwrForAll,
        FILE** pFp);

    bool DoBulkRecompress(ActionProgressDialog* pActionProgress,
        SelectionSet* pSelSet, const RecompressOptionsDialog* pRecompOpts);

    /*
     * Compute the total size of all files in the GenericArchive.
     */
    void CalcTotalSize(LONGLONG* pUncomp, LONGLONG* pComp) const;

    /*
     * Print a ContentList.
     */
    void PrintListing(const ContentList* pContentList);

    /*
     * ===== Clipboard.cpp =====
     */

    /*
     * Create a list of selected files.
     *
     * The returned string uses tab-delineated fields with CSV-style quoting
     * around the filename (so that double quotes in the filename don't confuse
     * applications like MS Excel).
     */
    CString CreateFileList(SelectionSet* pSelSet);

    /*
     * Double-up all double quotes.
     */
    static CString DblDblQuote(const WCHAR* str);

    /*
     * Compute the size of everything currently on the clipboard.
     */
    long GetClipboardContentLen(void);

    /*
     * Create the file collection.
     */
    HGLOBAL CreateFileCollection(SelectionSet* pSelSet);

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
    CString CopyToCollection(GenericEntry* pEntry, void** pBuf, long* pBufLen);

    /*
     * Do some prep work and then call ProcessClipboard to copy files in.
     */
    void DoPaste(bool pasteJunkPaths);

    /*
     * Process the data in the clipboard.
     *
     * Returns an empty string on success, or an error message on failure.
     */
    CString ProcessClipboard(const void* vbuf, long bufLen,
        bool pasteJunkPaths);

    /*
     * Process a single clipboard entry.
     *
     * On entry, "buf" points to the start of the first chunk of data (either
     * data fork or resource fork).  If the file has empty forks or is a
     * subdirectory, then "buf" is actually pointing at the start of the
     * next entry.
     */
    CString ProcessClipboardEntry(const FileCollectionEntry* pCollEnt,
        const WCHAR* pathName, const uint8_t* buf, long remLen);

    /*
     * ===== Tools.cpp =====
     */

    /*
     * Determines the settings we need to pass into DiskImgLib to create the
     * desired disk image format.
     *
     * Returns 0 on success, -1 on failure.
     */
    int DetermineImageSettings(int convertIdx, bool addGzip,
        DiskImg::OuterFormat* pOuterFormat, DiskImg::FileFormat* pFileFormat,
        DiskImg::PhysicalFormat* pPhysicalFormat,
        DiskImg::SectorOrder* pSectorOrder);

    /*
     * Converts one image during a bulk conversion.
     *
     * On failure, the reason for failure is stuffed into "*pErrMsg".
     */
    void BulkConvertImage(const WCHAR* pathName, const WCHAR* targetDir,
        const DiskConvertDialog& convDlg, CString* pErrMsg);

    /*
     * Opens one of the SST images.  Configures "pDiskImg" appropriately.
     *
     * Returns 0 on success, nonzero on failure.
     */
    int SSTOpenImage(int seqNum, DiskImg* pDiskImg);

    /*
     * Copies 17.5 tracks of data from the SST image to a .NIB image.
     *
     * Data is stored in all 16 sectors of track 0, followed by the first
     * 12 sectors of track 1, then on to track 2.  Total of $1a00 bytes.
     *
     * Returns 0 on success, -1 on failure.
     */
    int SSTLoadData(int seqNum, DiskImg* pDiskImg, uint8_t* trackBuf,
        long* pBadCount);

    /*
     * Compute the destination file offset for a particular source track.  The
     * track number ranges from 0 to 69 inclusive.  Sectors from two adjacent
     * "cooked" tracks are combined into a single "raw nibbilized" track.
     *
     * The data is ordered like this:
     *  track 1 sector 15 --> track 1 sector 4  (12 sectors)
     *  track 0 sector 13 --> track 0 sector 0  (14 sectors)
     *
     * Total of 26 sectors, or $1a00 bytes.
     */
    long SSTGetBufOffset(int track);

    /*
     * Count the number of "bad" bytes in the sector.
     *
     * Strictly speaking, a "bad" byte is anything that doesn't appear in the
     * 6&2 decoding table, 5&3 decoding table, special list (D5, AA), and
     * can't be used as a 4+4 encoding value.
     *
     * We just use $80 - $92, which qualify for all of the above.
     */
    long SSTCountBadBytes(const uint8_t* sctBuf, int count);

    /*
     * Run through the data, adding 0x80 everywhere and re-aligning the
     * tracks so that the big clump of sync bytes is at the end.
     */
    void SSTProcessTrackData(uint8_t* trackBuf);

    /*
     * Select a volume and then invoke the volcopy dialog.
     */
    void VolumeCopier(bool openFile);

    /*
     * Edit the properties of a 2MG file.
     *
     * Returns "true" if the file was modified, "false" if not.
     */
    bool EditTwoImgProps(const WCHAR* fileName);


    // set when one of the tools modifies the file we have open
    bool            fNeedReopen;

    CToolBar        fToolBar;
    CStatusBar      fStatusBar;

    // currently-open archive, if any
    GenericArchive* fpOpenArchive;
    // name of open archive, for display only -- if this is a temporary
    // file launched from another instance of CP, this won't be the name
    // of an actual file on disk.
    CString         fOpenArchivePathName;   // for display only

    // archive viewer, open when file is open
    // NOTE: make a super-class for a tree-structured display or other
    //  kinds of display, so we can avoid the if/then/else.  Rename
    //  ContentList to DetailList or FlatList or something.
    ContentList*    fpContentList;

    // currently selected set of goodies; used when viewing, extracting, etc.
    //SelectionSet* fpSelSet;

    // action progress meter, if any
    ActionProgressDialog*   fpActionProgress;

    // progress counter meter, if any
    ProgressCounterDialog*  fpProgressCounter;

    // modeless standard "find" dialog
    CFindReplaceDialog* fpFindDialog;
    CString         fFindLastStr;
    bool            fFindDown;
    bool            fFindMatchCase;
    bool            fFindMatchWholeWord;

    // our preferences
    Preferences     fPreferences;

    /*
     * Manage a list of files that must be deleted before we exit.
     */
    class DeleteList {
    private:
        class DeleteListNode {
        public:
            DeleteListNode(const CString& name) : fName(name),
                fPrev(NULL), fNext(NULL) {}
            ~DeleteListNode(void) {}

            DeleteListNode* fPrev;
            DeleteListNode* fNext;
            CString     fName;
        };

    public:
        DeleteList(void) { fHead = NULL; }
        ~DeleteList(void) {
            LOGD("Processing DeleteList (head=0x%p)", fHead);
            DeleteListNode* pNode = fHead;
            DeleteListNode* pNext;

            while (pNode != NULL) {
                pNext = pNode->fNext;
                if (_wunlink(pNode->fName) != 0) {
                    LOGW(" WARNING: delete of '%ls' failed, err=%d",
                        (LPCWSTR) pNode->fName, errno);
                } else {
                    LOGI(" Deleted '%ls'", (LPCWSTR) pNode->fName);
                }
                delete pNode;
                pNode = pNext;
            }
            LOGD("Processing DeleteList completed");
        }

        void Add(const CString& name) {
            DeleteListNode* pNode = new DeleteListNode(name);
            if (fHead != NULL) {
                fHead->fPrev = pNode;
                pNode->fNext = fHead;
            }
            fHead = pNode;
            LOGI("Delete-on-exit '%ls'", (LPCWSTR) name);
        }

        DeleteListNode* fHead;
    };
    DeleteList      fDeleteList;

    DECLARE_MESSAGE_MAP()
};

#define GET_MAIN_WINDOW() ((MainWindow*)::AfxGetMainWnd())

#define SET_PROGRESS_BEGIN() ((MainWindow*)::AfxGetMainWnd())->SetProgressBegin()
#define SET_PROGRESS_UPDATE(perc) \
    ((MainWindow*)::AfxGetMainWnd())->SetProgressUpdate(perc, NULL, NULL)
#define SET_PROGRESS_UPDATE2(perc, oldName, newName) \
    ((MainWindow*)::AfxGetMainWnd())->SetProgressUpdate(perc, oldName, newName)
#define SET_PROGRESS_END() ((MainWindow*)::AfxGetMainWnd())->SetProgressEnd()

#define SET_PROGRESS_COUNTER(val) \
    ((MainWindow*)::AfxGetMainWnd())->SetProgressCounter(NULL, val)
#define SET_PROGRESS_COUNTER_2(fmt, val) \
    ((MainWindow*)::AfxGetMainWnd())->SetProgressCounter(fmt, val)

#define GET_PREFERENCES() ((MainWindow*)::AfxGetMainWnd())->GetPreferences()
#define GET_PREFERENCES_WR() ((MainWindow*)::AfxGetMainWnd())->GetPreferencesWr()

#endif /*APP_MAIN_H*/
