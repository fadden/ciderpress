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
        fpDiskFS = nil;
        fFileName = "";
        fPositionShift = 0;
        fFirstResize = true;
    }
    virtual ~DiskEditDialog() {}

    void Setup(DiskFS* pDiskFS, const WCHAR* fileName) {
        ASSERT(pDiskFS != nil);
        ASSERT(fileName != nil);
        fpDiskFS = pDiskFS;
        fFileName = fileName;
    }

    enum { kSectorSize=256, kBlockSize=512 };

    virtual int LoadData(void) = 0;

    virtual void DisplayData(void) = 0;
    virtual void DisplayData(const BYTE* buf, int size);
    virtual void DisplayNibbleData(const BYTE* srcBuf, int size);

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

    // overrides
    virtual BOOL OnInitDialog(void);

    afx_msg virtual BOOL OnHelpInfo(HELPINFO* lpHelpInfo);
    afx_msg virtual void OnDone(void);
    afx_msg virtual void OnHexMode(void);
    afx_msg virtual void OnDoRead(void) = 0;
    afx_msg virtual void OnDoWrite(void) = 0;
    afx_msg virtual void OnReadPrev(void) = 0;
    afx_msg virtual void OnReadNext(void) = 0;
    afx_msg virtual void OnSubVolume(void);
    afx_msg virtual void OnOpenFile(void) = 0;
    afx_msg virtual void OnNibbleParms(void);
    afx_msg virtual void OnHelp(void);

    virtual BOOL PreTranslateMessage(MSG* pMsg);

    void SetSpinMode(int id, int base);
    int ReadSpinner(int id, long* pVal);
    void SetSpinner(int id, long val);

    //void FillWithPattern(unsigned char* buf, int size, const char* pattern);
    DIError OpenFile(const WCHAR* fileName, bool openRsrc, A2File** ppFile,
        A2FileDescr** ppOpenFile);

    DiskFS*     fpDiskFS;
    CString     fFileName;
    CString     fAlertMsg;
    bool        fReadOnly;
    int         fPositionShift;

private:
    void InitNibbleParmList(void);
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

    virtual int LoadData(void);     // load the current track/sector
    virtual void DisplayData(void) {
        DiskEditDialog::DisplayData(fSectorData, kSectorSize);
    }

    //void SetTrack(int val) { fTrack = val; }
    //void SetSector(int val) { fSector = val; }

    // overrides
    virtual BOOL OnInitDialog(void);

protected:
    afx_msg virtual void OnDoRead(void);
    afx_msg virtual void OnDoWrite(void);
    afx_msg virtual void OnReadPrev(void);
    afx_msg virtual void OnReadNext(void);
    afx_msg virtual void OnOpenFile(void);

    long        fTrack;
    long        fSector;
    BYTE        fSectorData[kSectorSize];
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

    virtual int LoadData(void);     // load the current block
    virtual void DisplayData(void) {
        DiskEditDialog::DisplayData(fBlockData, kBlockSize);
    }

    // overrides
    virtual BOOL OnInitDialog(void);

protected:
    //void MoveControl(int id, int deltaX, int deltaY);

    afx_msg virtual void OnDoRead(void);
    afx_msg virtual void OnDoWrite(void);
    afx_msg virtual void OnReadPrev(void);
    afx_msg virtual void OnReadNext(void);
    afx_msg virtual void OnOpenFile(void);

    long        fBlock;
    BYTE        fBlockData[kBlockSize];
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

    afx_msg virtual void OnReadPrev(void);
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

    virtual int LoadData(void);     // load the current track/sector
    virtual void DisplayData(void) {
        DiskEditDialog::DisplayNibbleData(fNibbleData, fNibbleDataLen);
    }

    // overrides
    virtual BOOL OnInitDialog(void);

protected:
    afx_msg virtual void OnDoRead(void);
    afx_msg virtual void OnDoWrite(void);
    afx_msg virtual void OnReadPrev(void);
    afx_msg virtual void OnReadNext(void);
    afx_msg virtual void OnOpenFile(void) { ASSERT(false); }
    afx_msg virtual void OnNibbleParms(void) { ASSERT(false); }

    long        fTrack;
    BYTE        fNibbleData[DiskImgLib::kTrackAllocSize];
    long        fNibbleDataLen;
};

#endif /*APP_DISKEDITDIALOG_H*/
