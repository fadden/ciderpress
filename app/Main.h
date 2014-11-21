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

typedef enum {
    kFilterIndexNuFX = 1,
    kFilterIndexBinaryII = 2,
    kFilterIndexACU = 3,
    kFilterIndexDiskImage = 4,
    kFilterIndexGeneric = 5,        // *.* filter used
} FilterIndex;

struct FileCollectionEntry;     // fwd

/*
 * The main UI window.
 */
class MainWindow : public CFrameWnd
{
public:
    MainWindow(void);
    ~MainWindow(void);

    // Overridden functions
    BOOL PreCreateWindow(CREATESTRUCT& cs);
    //BOOL OnCreateClient( LPCREATESTRUCT lpcs, CCreateContext* pContext );
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

    // update the progress counter
    bool SetProgressCounter(const WCHAR* fmt, long val);

    // handle a double-click in the content view
    void HandleDoubleClick(void);

    // do some idle processing
    void DoIdle(void);

    // return the title to put at the top of a printout
    CString GetPrintTitle(void);

    // raise flag to abort the current print job
    void SetAbortPrinting(bool val) { fAbortPrinting = val; }
    bool GetAbortPrinting(void) const { return fAbortPrinting; }
    static BOOL CALLBACK PrintAbortProc(HDC hDC, int nCode);
    bool            fAbortPrinting;
    // track printer choice
    HANDLE          fhDevMode;
    HANDLE          fhDevNames;

    // set flag to abort current operation
    //void SetAbortOperation(bool val) { fAbortOperation = val; }
    //bool          fAbortOperation;

    // pause, for debugging
    void EventPause(int duration);

    ContentList* GetContentList(void) const { return fpContentList; }

    void SetActionProgressDialog(ActionProgressDialog* pActionProgress) {
        fpActionProgress = pActionProgress;
    }
    void SetProgressCounterDialog(ProgressCounterDialog* pProgressCounter) {
        fpProgressCounter = pProgressCounter;
    }
    GenericArchive* GetOpenArchive(void) const { return fpOpenArchive; }

    int GetFileParts(const GenericEntry* pEntry,
        ReformatHolder** ppHolder) const;

    // force processing of pending messages
    BOOL PeekAndPump();

    // make a happy noise after successful execution of a command
    void SuccessBeep(void);
    // make a not-so-happy noise
    void FailureBeep(void);

    // remove a file, returning a helpful message on failure
    CString RemoveFile(const WCHAR* fileName);

    // choose the place to put a file
    bool ChooseAddTarget(DiskImgLib::A2File** ppTargetSubdir,
        DiskImgLib::DiskFS** ppDiskFS);

    // try a disk image override dialog
    int TryDiskImgOverride(DiskImg* pImg, const WCHAR* fileSource,
        DiskImg::FSFormat defaultFormat, int* pDisplayFormat,
        bool allowUnknown, CString* pErrMsg);
    // copy all blocks from one disk image to another
    DIError CopyDiskImage(DiskImg* pDstImg, DiskImg* pSrcImg, bool bulk,
        bool partial, ProgressCancelDialog* pPCDialog);

    // does the currently open archive pathname match?
    bool IsOpenPathName(const WCHAR* path);
    // raise a flag to cause a full reload of the open file
    void SetReopenFlag(void) { fNeedReopen = true; }

    static void ConfigureReformatFromPreferences(ReformatHolder* pReformat);
    static ReformatHolder::SourceFormat ReformatterSourceFormat(DiskImg::FSFormat format);

    // save a buffer of data as a file in a disk image or file archive
    static bool SaveToArchive(GenericArchive::FileDetails* pDetails,
        const uint8_t* dataBuf, long dataLen,
        const uint8_t* rsrcBuf, long rsrcLen,
        CString& errMsg, CWnd* pDialog);

    static const WCHAR kOpenNuFX[];
    static const WCHAR kOpenBinaryII[];
    static const WCHAR kOpenACU[];
    static const WCHAR kOpenDiskImage[];
    static const WCHAR kOpenAll[];
    static const WCHAR kOpenEnd[];

private:
    static const WCHAR kModeNuFX[];
    static const WCHAR kModeBinaryII[];
    static const WCHAR kModeACU[];
    static const WCHAR kModeDiskImage[];

    // Command handlers
    afx_msg int OnCreate(LPCREATESTRUCT lpcs);
    afx_msg LONG OnLateInit(UINT, LONG);
    //afx_msg LONG OnCloseMainDialog(UINT, LONG);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* pMMI);
    afx_msg void OnPaint(void);
    //afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo);
    afx_msg BOOL OnQueryEndSession(void);
    afx_msg void OnEndSession(BOOL bEnding);
    afx_msg LRESULT OnFindDialogMessage(WPARAM wParam, LPARAM lParam);
    //afx_msg LONG OnHelp(UINT wParam, LONG lParam);
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
    afx_msg void OnEditCopy(void);
    afx_msg void OnUpdateEditCopy(CCmdUI* pCmdUI);
    afx_msg void OnEditPaste(void);
    afx_msg void OnUpdateEditPaste(CCmdUI* pCmdUI);
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
    afx_msg void OnActionsView(void);
    afx_msg void OnUpdateActionsView(CCmdUI* pCmdUI);
    afx_msg void OnActionsOpenAsDisk(void);
    afx_msg void OnUpdateActionsOpenAsDisk(CCmdUI* pCmdUI);
    afx_msg void OnActionsAddFiles(void);
    afx_msg void OnUpdateActionsAddFiles(CCmdUI* pCmdUI);
    afx_msg void OnActionsAddDisks(void);
    afx_msg void OnUpdateActionsAddDisks(CCmdUI* pCmdUI);
    afx_msg void OnActionsCreateSubdir(void);
    afx_msg void OnUpdateActionsCreateSubdir(CCmdUI* pCmdUI);
    afx_msg void OnActionsExtract(void);
    afx_msg void OnUpdateActionsExtract(CCmdUI* pCmdUI);
    afx_msg void OnActionsTest(void);
    afx_msg void OnUpdateActionsTest(CCmdUI* pCmdUI);
    afx_msg void OnActionsDelete(void);
    afx_msg void OnUpdateActionsDelete(CCmdUI* pCmdUI);
    afx_msg void OnActionsRename(void);
    afx_msg void OnUpdateActionsRename(CCmdUI* pCmdUI);
    afx_msg void OnActionsEditComment(void);
    afx_msg void OnUpdateActionsEditComment(CCmdUI* pCmdUI);
    afx_msg void OnActionsEditProps(void);
    afx_msg void OnUpdateActionsEditProps(CCmdUI* pCmdUI);
    afx_msg void OnActionsRenameVolume(void);
    afx_msg void OnUpdateActionsRenameVolume(CCmdUI* pCmdUI);
    afx_msg void OnActionsRecompress(void);
    afx_msg void OnUpdateActionsRecompress(CCmdUI* pCmdUI);
    afx_msg void OnActionsConvDisk(void);
    afx_msg void OnUpdateActionsConvDisk(CCmdUI* pCmdUI);
    afx_msg void OnActionsConvFile(void);
    afx_msg void OnUpdateActionsConvFile(CCmdUI* pCmdUI);
    afx_msg void OnActionsConvToWav(void);
    afx_msg void OnUpdateActionsConvToWav(CCmdUI* pCmdUI);
    afx_msg void OnActionsConvFromWav(void);
    afx_msg void OnUpdateActionsConvFromWav(CCmdUI* pCmdUI);
    afx_msg void OnActionsImportBAS(void);
    afx_msg void OnUpdateActionsImportBAS(CCmdUI* pCmdUI);
    afx_msg void OnToolsDiskEdit(void);
    afx_msg void OnToolsDiskConv(void);
    afx_msg void OnToolsBulkDiskConv(void);
    afx_msg void OnToolsSSTMerge(void);
    afx_msg void OnToolsVolumeCopierVolume(void);
    afx_msg void OnToolsVolumeCopierFile(void);
    afx_msg void OnToolsEOLScanner(void);
    afx_msg void OnToolsTwoImgProps(void);
    afx_msg void OnToolsDiskImageCreator(void);
    afx_msg void OnHelpContents(void);
    afx_msg void OnHelpWebSite(void);
    afx_msg void OnHelpOrdering(void);
    afx_msg void OnHelpAbout(void);
    afx_msg void OnRtClkDefault(void);

    void ProcessCommandLine(void);
    void ResizeClientArea(void);
    void DrawEmptyClientArea(CDC* pDC, const CRect& clientRect);
    int TmpExtractAndOpen(GenericEntry* pEntry, int threadKind,
        const WCHAR* modeStr);
    int TmpExtractForExternal(GenericEntry* pEntry);
    void DoOpenArchive(const WCHAR* pathName, const WCHAR* ext,
        int filterIndex, bool readOnly);
    int LoadArchive(const WCHAR* filename, const WCHAR* extension,
            int filterIndex, bool readOnly, bool createFile);
    int DoOpenVolume(CString drive, bool readOnly);
    void SwitchContentList(GenericArchive* pOpenArchive);
    void CloseArchiveWOControls(void);
    void CloseArchive(void);
    void SetCPTitle(const WCHAR* pathname, GenericArchive* pArchive);
    void SetCPTitle(void);
    GenericEntry* GetSelectedItem(ContentList* pContentList);
    void HandleView(void);

    void DeleteFileOnExit(const WCHAR* name);

    void ReopenArchive(void);

    /* some stuff from Actions.cpp */
    //int GetFileText(SelectionEntry* pSelEntry, ReformatHolder* pHolder,
    //  CString* pTitle);
    void GetFilePart(const GenericEntry* pEntry, int whichThread,
        ReformatHolder* pHolder) const;

    /**
    * this is a test
    * of whatever the hell this does
    * whee.
    */
    void DoBulkExtract(SelectionSet* pSelSet,
        const ExtractOptionsDialog* pExtOpts);
    bool ExtractEntry(GenericEntry* pEntry, int thread,
        ReformatHolder* pHolder, const ExtractOptionsDialog* pExtOpts,
        bool* pOverwriteExisting, bool* pOvwrForAll);
    int OpenOutputFile(CString* pOutputPath, const PathProposal& pathProp,
        time_t arcFileModWhen, bool* pOverwriteExisting, bool* pOvwrForAll,
        FILE** pFp);
    bool DoBulkRecompress(ActionProgressDialog* pActionProgress,
        SelectionSet* pSelSet, const RecompressOptionsDialog* pRecompOpts);
    void CalcTotalSize(LONGLONG* pUncomp, LONGLONG* pComp) const;

    /* some stuff from Clipboard.cpp */
    CString CreateFileList(SelectionSet* pSelSet);
    static CString DblDblQuote(const WCHAR* str);
    long GetClipboardContentLen(void);
    HGLOBAL CreateFileCollection(SelectionSet* pSelSet);
    CString CopyToCollection(GenericEntry* pEntry, void** pBuf, long* pBufLen);
    void DoPaste(bool pasteJunkPaths);
    CString ProcessClipboard(const void* vbuf, long bufLen,
        bool pasteJunkPaths);
    CString ProcessClipboardEntry(const FileCollectionEntry* pCollEnt,
        const WCHAR* pathName, const unsigned char* buf, long remLen);

    /* some stuff from Tools.cpp */
    int DetermineImageSettings(int convertIdx, bool addGzip,
        DiskImg::OuterFormat* pOuterFormat, DiskImg::FileFormat* pFileFormat,
        DiskImg::PhysicalFormat* pPhysicalFormat,
        DiskImg::SectorOrder* pSectorOrder);
    void BulkConvertImage(const WCHAR* pathName, const WCHAR* targetDir,
        const DiskConvertDialog& convDlg, CString* pErrMsg);
    int SSTOpenImage(int seqNum, DiskImg* pDiskImg);
    int SSTLoadData(int seqNum, DiskImg* pDiskImg, uint8_t* trackBuf,
        long* pBadCount);
    long SSTGetBufOffset(int track);
    long SSTCountBadBytes(const uint8_t* sctBuf, int count);
    void SSTProcessTrackData(uint8_t* trackBuf);
    void VolumeCopier(bool openFile);
    bool EditTwoImgProps(const WCHAR* fileName);


    void PrintListing(const ContentList* pContentList);

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
