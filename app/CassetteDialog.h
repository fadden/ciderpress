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
    virtual BOOL OnInitDialog(void) override;
    //virtual void DoDataExchange(CDataExchange* pDX);
    //virtual void OnOK(void);
    
    //enum { WMU_DIALOG_READY = WM_USER+2 };

    /*
     * Something changed in the list.  Update the "OK" button.
     */
    afx_msg void OnListChange(NMHDR* pNotifyStruct, LRESULT* pResult);

    /*
     * The volume filter drop-down box has changed.
     */
    afx_msg void OnAlgorithmChange(void);

    /*
     * User pressed "import" button.  Add the selected item to the current
     * archive or disk image.
     */
    afx_msg void OnImport(void);

    afx_msg void OnListDblClick(NMHDR* pNotifyStruct, LRESULT* pResult);
    afx_msg void OnHelp(void) {
        MyApp::HandleHelp(this, HELP_TOPIC_IMPORT_CASSETTE);
    }

    /*
     * This holds converted data from the WAV file, plus some meta-data
     * like what type of file we think this is.
     */
    class CassetteData {
    public:
        CassetteData(void) : fFileType(0x00), fOutputBuf(NULL), fOutputLen(-1),
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

        /*
         * Scan the WAV file, starting from the specified byte offset.
         *
         * Returns "true" if we found a file, "false" if not (indicating that the
         * end of the input has been reached).  Updates "*pStartOffset" to point
         * past the end of the data we've read.
         */
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

        /*
         * Convert a block of samples from PCM to float.
         *
         * Only the first (left) channel is converted in multi-channel formats.
         */
        void ConvertSamplesToReal(const WAVEFORMATEX* pFormat,
            const unsigned char* buf, long chunkLen, float* sampleBuf);

        /*
         * Process one audio sample.  Updates "pScanState" appropriately.
         *
         * If we think we found a bit, this returns "true" with 0 or 1 in "*pBitVal".
         */
        bool ProcessSample(float sample, long sampleIndex,
            ScanState* pScanState, int* pBitVal);

        /*
         * Process the data by measuring the distance between zero crossings.
         *
         * This is very similar to the way the Apple II does it, though
         * we have to scan for the 770Hz lead-in instead of simply assuming the
         * the user has queued up the tape.
         *
         * To offset the effects of DC bias, we examine full cycles instead of
         * half cycles.
         */
        bool ProcessSampleZero(float sample, long sampleIndex,
            ScanState* pScanState, int* pBitVal);

        /*
         * Process the data by finding and measuring the distance between peaks.
         */
        bool ProcessSamplePeak(float sample, long sampleIndex,
            ScanState* pScanState, int* pBitVal);

        /*
         * Given the width of a half-cycle, update "phase" and decide whether or not
         * it's time to emit a bit.
         *
         * Updates "halfCycleWidth" too, alternating between 0.0 and a value.
         *
         * The "sampleIndex" parameter is largely just for display.  We use it to
         * set the "start" and "end" pointers, but those are also ultimately just
         * for display to the user.
         */
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

    /*
     * Analyze the contents of a WAV file.
     *
     * Returns true if it found anything at all, false if not.
     */
    bool AnalyzeWAV(void);

    /*
     * Add an entry to the list.
     *
     * Layout: index format length checksum start-offset
     */
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
