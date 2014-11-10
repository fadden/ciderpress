/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Apple II cassette I/O functions.
 */
#ifndef APP_CASSETTEDIALOG_H
#define APP_CASSETTEDIALOG_H

/*
 * The dialog box is primarily concerned with extracting the original data
 * from a WAV file recording of an Apple II cassette tape.
 */
class CassetteDialog : public CDialog {
public:
    CassetteDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_IMPORTCASSETTE, pParentWnd), fDirty(false)
        {}
    virtual ~CassetteDialog(void) {}

    CString fFileName;      // file to open

    bool IsDirty(void) const { return fDirty; }

private:
    virtual BOOL OnInitDialog(void);
    //virtual void DoDataExchange(CDataExchange* pDX);
    //virtual void OnOK(void);
    
    //enum { WMU_DIALOG_READY = WM_USER+2 };

    afx_msg void OnListChange(NMHDR* pNotifyStruct, LRESULT* pResult);
    afx_msg void OnListDblClick(NMHDR* pNotifyStruct, LRESULT* pResult);
    afx_msg void OnAlgorithmChange(void);
    afx_msg void OnHelp(void);
    afx_msg void OnImport(void);

    /*
     * This holds converted data from the WAV file, plus some meta-data
     * like what type of file we think this is.
     */
    class CassetteData {
    public:
        CassetteData(void) : fFileType(0x00), fOutputBuf(nil), fOutputLen(-1),
            fStartSample(-1), fEndSample(-1), fChecksum(0x00),
            fChecksumGood(false)
            {}
        virtual ~CassetteData(void) { delete[] fOutputBuf; }

        /*
         * Algorithm to use.  This must match up with the order of the items
         * in the dialog IDC_CASSETTE_ALG combo box.
         */
        typedef enum Algorithm {
            kAlgorithmMIN = -1,

            kAlgorithmZero = 0,
            kAlgorithmSharpPeak,
            kAlgorithmRoundPeak,
            kAlgorithmShallowPeak,

            kAlgorithmMAX
        } Algorithm;

        bool Scan(SoundFile* pSoundFile, Algorithm alg, long* pSampleOffset);
        unsigned char* GetDataBuf(void) const { return fOutputBuf; }
        int GetDataLen(void) const { return fOutputLen; }
        int GetDataOffset(void) const { return fStartSample; }
        int GetDataEndOffset(void) const { return fEndSample; }
        unsigned char GetDataChecksum(void) const { return fChecksum; }
        bool GetDataChkGood(void) const { return fChecksumGood; }

        long GetFileType(void) const { return fFileType; }
        void SetFileType(long fileType) { fFileType = fileType; }

    private:
        typedef enum Phase {
            kPhaseUnknown = 0,
            kPhaseScanFor770Start,
            kPhaseScanning770,
            kPhaseScanForShort0,
            kPhaseShort0B,
            kPhaseReadData,
            kPhaseEndReached,
        //  kPhaseError,
        } Phase;
        typedef enum Mode {
            kModeUnknown = 0,
            kModeInitial0,
            kModeInitial1,

            kModeInTransition,
            kModeAtPeak,

            kModeRunning,
        } Mode;

        typedef struct ScanState {
            Algorithm algorithm;
            Phase   phase;
            Mode    mode;
            bool    positive;           // rising or at +peak if true

            long    lastZeroIndex;      // in samples
            long    lastPeakStartIndex; // in samples
            float   lastPeakStartValue;

            float   prevSample;

            float   halfCycleWidth;     // in usec
            long    num770;             // #of consecutive 770Hz cycles
            long    dataStart;
            long    dataEnd;

            /* constants */
            float   usecPerSample;
        } ScanState;
        void ConvertSamplesToReal(const WAVEFORMATEX* pFormat,
            const unsigned char* buf, long chunkLen, float* sampleBuf);
        bool ProcessSample(float sample, long sampleIndex,
            ScanState* pScanState, int* pBitVal);
        bool ProcessSampleZero(float sample, long sampleIndex,
            ScanState* pScanState, int* pBitVal);
        bool ProcessSamplePeak(float sample, long sampleIndex,
            ScanState* pScanState, int* pBitVal);
        bool UpdatePhase(ScanState* pScanState, long sampleIndex,
            float halfCycleUsec, int* pBitVal);

        enum {
            kMaxFileLen = 65535+2+1+1,  // 64K + length + checksum + 1 slop
        };

        long            fFileType;      // 0x06, 0xfa, or 0xfc
        unsigned char*  fOutputBuf;
        int             fOutputLen;
        long            fStartSample;
        long            fEndSample;
        unsigned char   fChecksum;
        bool            fChecksumGood;
    };

    bool AnalyzeWAV(void);
    void AddEntry(int idx, CListCtrl* pListCtrl, long* pFileType);

    enum {
        kMaxRecordings = 100,       // max A2 files per WAV file
    };

    /* array with one entry per file */
    CassetteData    fDataArray[kMaxRecordings];

    CassetteData::Algorithm fAlgorithm;
    bool    fDirty;

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_CASSETTEDIALOG_H*/
