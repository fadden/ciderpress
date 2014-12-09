/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Class definition for DiskEdit dialog.
 */
#ifndef APP_DISKEDITDIALOG_H
#define APP_DISKEDITDIALOG_H

#include "../diskimg/DiskImg.h"
#include "../util/UtilLib.h"
#include "resource.h"

/*
 * An abstract base class to support "sector editing" and "block editing"
 * dialogs, which differ chiefly in how much data they present at a time.
 *
 * NOTE: override OnCancel to insert an "are you sure" message when the
 * block is dirty.
 *
 * NOTE to self: if the initial block/sector read fails, we can be left
 * with invalid stuff in the buffer.  Keep that in mind if editing is
 * enabled.
 */
class DiskEditDialog : public CDialog {
public:
    DiskEditDialog(UINT nIDTemplate, CWnd* pParentWnd = NULL) :
        CDialog(nIDTemplate, pParentWnd)
    {
        fReadOnly = true;
        fpDiskFS = NULL;
        fFileName = "";
        fPositionShift = 0;
        fFirstResize = true;
    }
    virtual ~DiskEditDialog() {}

    void Setup(DiskFS* pDiskFS, const WCHAR* fileName) {
        ASSERT(pDiskFS != NULL);
        ASSERT(fileName != NULL);
        fpDiskFS = pDiskFS;
        fFileName = fileName;
    }

    enum { kSectorSize=256, kBlockSize=512 };

    virtual int LoadData(void) = 0;

    virtual void DisplayData(void) = 0;

    /*
     * Convert a chunk of data into a hex dump, and stuff it into the edit control.
     */
    virtual void DisplayData(const uint8_t* buf, int size);

    /*
     * Display a track full of nibble data.
     */
    virtual void DisplayNibbleData(const uint8_t* srcBuf, int size);

    bool GetReadOnly(void) const { return fReadOnly; }
    void SetReadOnly(bool val) { fReadOnly = val; }
    int GetPositionShift(void) const { return fPositionShift; }
    void SetPositionShift(int val) { fPositionShift = val; }
    DiskFS* GetDiskFS(void) const { return fpDiskFS; }
    const WCHAR* GetFileName(void) const { return fFileName; }

protected:
    // return a low-ASCII character so we can read high-ASCII files
    inline char PrintableChar(unsigned char ch) {
        if (ch < 0x20)
            return '.';
        else if (ch < 0x80)
            return ch;
        else if (ch < 0xa0 || ch == 0xff)   // 0xff becomes 0x7f
            return '.';
        else
            return ch & 0x7f;
    }

    virtual BOOL OnInitDialog(void) override;

    // catch <return> key
    virtual BOOL PreTranslateMessage(MSG* pMsg) override;

    /*
     * Handle the "Done" button.  We don't use IDOK because we don't want
     * <return> to bail out of the dialog.
     */
    afx_msg virtual void OnDone(void);

     /*
      * Toggle the spin button / edit controls.
      */
    afx_msg virtual void OnHexMode(void);

    afx_msg virtual void OnDoRead(void) = 0;
    afx_msg virtual void OnDoWrite(void) = 0;
    afx_msg virtual void OnReadPrev(void) = 0;
    afx_msg virtual void OnReadNext(void) = 0;

    /*
     * Create a new instance of the disk edit dialog, for a sub-volume.
     */
    afx_msg virtual void OnSubVolume(void);

    afx_msg virtual void OnOpenFile(void) = 0;

    /*
     * Change the nibble parms.
     *
     * Assumes the parm list is linear and unbroken.
     */
    afx_msg virtual void OnNibbleParms(void);

    afx_msg void OnHelp(void) {
        MyApp::HandleHelp(this, HELP_TOPIC_DISKEDIT);
    }
    afx_msg BOOL OnHelpInfo(HELPINFO* lpHelpInfo) {
        return MyApp::HandleHelpInfo(lpHelpInfo);
    }

    /*
     * Change the mode of a spin button.  The Windows control doesn't
     * immediately update with a hex display, so we do it manually.  (Our
     * replacement class does this correctly, but I'm leaving the code alone
     * for now.)
     */
    void SetSpinMode(int id, int base);

    /*
     * Read a value from a spin control.
     *
     * Returns 0 on success, -1 if the return value from the spin control was
     * invalid.  In the latter case, an error dialog will be displayed.
     */
    int ReadSpinner(int id, long* pVal);

    /*
     * Set the value of a spin control.
     */
    void SetSpinner(int id, long val);

    /*
     * Open a file in a disk image.
     *
     * Returns a pointer to the A2File and A2FileDescr structures on success, NULL
     * on failure.  The pointer placed in "ppOpenFile" must be freed by invoking
     * its Close function.
     */
    DIError OpenFile(const WCHAR* fileName, bool openRsrc, A2File** ppFile,
        A2FileDescr** ppOpenFile);

    DiskFS*     fpDiskFS;
    CString     fFileName;
    CString     fAlertMsg;
    bool        fReadOnly;
    int         fPositionShift;

private:
    /*
     * Initialize the nibble parm drop-list.
     */
    void InitNibbleParmList(void);

    /*
     * Replace a spin button with our improved version.
     */
    int ReplaceSpinCtrl(MySpinCtrl* pNewSpin, int idSpin, int idEdit);

    MySpinCtrl  fTrackSpinner;
    MySpinCtrl  fSectorSpinner;
    bool        fFirstResize;

    //afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()
};


/*
 * The "sector edit" dialog, which displays 256 bytes at a time, and
 * accesses a disk by track/sector.
 */
class SectorEditDialog : public DiskEditDialog {
public:
    SectorEditDialog(CWnd* pParentWnd = NULL) :
        DiskEditDialog(IDD_DISKEDIT, pParentWnd)
    {
        fTrack = 0;
        fSector = 0;
    }
    virtual ~SectorEditDialog() {}

    virtual int LoadData(void) override;    // load the current track/sector
    virtual void DisplayData(void) override {
        DiskEditDialog::DisplayData(fSectorData, kSectorSize);
    }

    //void SetTrack(int val) { fTrack = val; }
    //void SetSector(int val) { fSector = val; }

protected:
    virtual BOOL OnInitDialog(void) override;

    afx_msg virtual void OnDoRead(void);
    afx_msg virtual void OnDoWrite(void);

    /*
     * Back up to the previous track/sector.
     */
    afx_msg virtual void OnReadPrev(void);

    /*
     * Advance to the next track/sector.
     */
    afx_msg virtual void OnReadNext(void);

    /*
     * Open a file on the disk image.  If successful, open a new edit dialog
     * that's in "file follow" mode.
     */
    afx_msg virtual void OnOpenFile(void);

    long        fTrack;
    long        fSector;
    uint8_t     fSectorData[kSectorSize];
};

/*
 * Edit a file sector-by-sector.
 */
class SectorFileEditDialog : public SectorEditDialog {
public:
    SectorFileEditDialog(SectorEditDialog* pSectEdit, CWnd* pParentWnd = NULL):
        SectorEditDialog(pParentWnd)
    {
        DiskEditDialog::Setup(pSectEdit->GetDiskFS(),
            pSectEdit->GetFileName());
        fSectorIdx = 0;
    }
    virtual ~SectorFileEditDialog() {}

    /* we do NOT own pOpenFile, and should not delete it */
    void SetupFile(const WCHAR* fileName, bool rsrcFork, A2File* pFile,
        A2FileDescr* pOpenFile)
    {
        fOpenFileName = fileName;
        fOpenRsrcFork = rsrcFork;
        fpFile = pFile;
        fpOpenFile = pOpenFile;
        fLength = 0;
        if (fpOpenFile->Seek(0, DiskImgLib::kSeekEnd) == kDIErrNone)
            fLength = fpOpenFile->Tell();
    }

    virtual int LoadData(void);     // load data from the current offset

private:
    // overrides
    virtual BOOL OnInitDialog(void);

    afx_msg virtual void OnReadPrev(void);
    afx_msg virtual void OnReadNext(void);

    CString     fOpenFileName;
    bool        fOpenRsrcFork;
    A2File*     fpFile;
    A2FileDescr* fpOpenFile;
    //long      fOffset;
    long        fSectorIdx;
    di_off_t    fLength;
};


/*
 * The "block edit" dialog, which displays 512 bytes at a time, and
 * accesses a disk by linear block number.
 */
class BlockEditDialog : public DiskEditDialog {
public:
    BlockEditDialog(CWnd* pParentWnd = NULL) :
        DiskEditDialog(IDD_DISKEDIT, pParentWnd)
    {
        fBlock = 0;
    }
    virtual ~BlockEditDialog() {}

    virtual int LoadData(void) override;    // load the current block
    virtual void DisplayData(void) override {
        DiskEditDialog::DisplayData(fBlockData, kBlockSize);
    }

protected:
    virtual BOOL OnInitDialog(void) override;

    afx_msg virtual void OnDoRead(void);
    afx_msg virtual void OnDoWrite(void);

    /*
     * Back up to the previous track/sector, or (in follow-file mode) to the
     * previous sector in the file.
     */
    afx_msg virtual void OnReadPrev(void);

    /*
     * Same as OnReadPrev, but moving forward.
     */
    afx_msg virtual void OnReadNext(void);

    /*
     * Open a file on the disk image.  If successful, open a new edit dialog
     * that's in "file follow" mode.
     */
    afx_msg virtual void OnOpenFile(void);

    long        fBlock;
    uint8_t     fBlockData[kBlockSize];
};


/*
 * Edit a file block-by-block.
 */
class BlockFileEditDialog : public BlockEditDialog {
public:
    BlockFileEditDialog(BlockEditDialog* pBlockEdit, CWnd* pParentWnd = NULL) :
        BlockEditDialog(pParentWnd)
    {
        DiskEditDialog::Setup(pBlockEdit->GetDiskFS(),
            pBlockEdit->GetFileName());
        fBlockIdx = 0;
    }
    virtual ~BlockFileEditDialog() {}

    /* we do NOT own pOpenFile, and should not delete it */
    void SetupFile(const WCHAR* fileName, bool rsrcFork, A2File* pFile,
        A2FileDescr* pOpenFile)
    {
        fOpenFileName = fileName;
        fOpenRsrcFork = rsrcFork;
        fpFile = pFile;
        fpOpenFile = pOpenFile;
        fLength = 0;
        if (fpOpenFile->Seek(0, DiskImgLib::kSeekEnd) == kDIErrNone)
            fLength = fpOpenFile->Tell();
    }

    virtual int LoadData(void);     // load data from the current offset

private:
    // overrides
    virtual BOOL OnInitDialog(void);

    /*
     * Move to the previous Block in the file.
     */
    afx_msg virtual void OnReadPrev(void);

    /*
     * Move to the next Block in the file.
     */
    afx_msg virtual void OnReadNext(void);

    CString     fOpenFileName;
    bool        fOpenRsrcFork;
    A2File*     fpFile;
    A2FileDescr* fpOpenFile;
    //long      fOffset;
    long        fBlockIdx;
    di_off_t    fLength;
};

/*
 * The "sector edit" dialog, which displays 256 bytes at a time, and
 * accesses a disk by track/sector.
 */
class NibbleEditDialog : public DiskEditDialog {
public:
    NibbleEditDialog(CWnd* pParentWnd = NULL) :
        DiskEditDialog(IDD_DISKEDIT, pParentWnd)
    {
        fTrack = 0;
    }
    virtual ~NibbleEditDialog() {}

    virtual int LoadData(void) override;    // load the current track/sector
    virtual void DisplayData(void) override {
        DiskEditDialog::DisplayNibbleData(fNibbleData, fNibbleDataLen);
    }

protected:
    /*
     * Rearrange the DiskEdit dialog (which defaults to SectorEdit mode) to
     * accommodate nibble editing.
     */
    virtual BOOL OnInitDialog(void) override;

    afx_msg virtual void OnDoRead(void);
    afx_msg virtual void OnDoWrite(void);
    afx_msg virtual void OnReadPrev(void);
    afx_msg virtual void OnReadNext(void);
    afx_msg virtual void OnOpenFile(void) { ASSERT(false); }
    afx_msg virtual void OnNibbleParms(void) { ASSERT(false); }

    long        fTrack;
    uint8_t     fNibbleData[DiskImgLib::kTrackAllocSize];
    long        fNibbleDataLen;
};

#endif /*APP_DISKEDITDIALOG_H*/
