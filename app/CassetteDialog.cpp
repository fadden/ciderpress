/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Apple II cassette I/O functions.
 */
#include "StdAfx.h"
#include "CassetteDialog.h"
#include "CassImpTargetDialog.h"
#include "GenericArchive.h"
#include "Main.h"
#include "../diskimg/DiskImg.h"     // need kStorageSeedling
#include <math.h>

/*
 * Tape layout:
 *  10.6 seconds of 770Hz (8192 cycles * 1300 usec/cycle)
 *  1/2 cycle at 400 usec/cycle, followed by 1/2 cycle at 500 usec/cycle
 *  Data, using 500 usec/cycle for '0' and 1000 usec/cycle for '1'
 *  There is no "end" marker, except perhaps for the absence of data
 *
 * The last byte of data is an XOR checksum (seeded with 0xff).
 *
 * BASIC uses two sections, each with the full 10-second lead-in and a
 * checksum byte).  Integer BASIC writes a two-byte section with the length
 * of the program, while Applesoft BASIC writes a three-byte section with
 * the length followed by a one-byte "run" flag (seen: 0x55 and 0xd5).
 *
 * Applesoft arrays, loaded with "RECALL", have a three-byte header, and
 * may be confused with BASIC programs.  Shape tables, loaded with "SHLOAD",
 * have a two-byte header and may be confused with Integer programs.
 *
 * The monitor ROM routine uses a detection threshold of 700 usec to tell
 * the difference between 0s and 1s.  When reading, it *outputs* a tone for
 * 3.5 seconds before listening.  It doesn't try to detect the 770Hz tone,
 * just waits for something under (40*12=)440 usec.
 *
 * The Apple II hardware changes the high bit read from $c060 every time it
 * detects a zero-crossing on the cassette input.  I assume the polarity
 * of the input signal is reflected by the polarity of the high bit, but
 * I'm not sure, and in the end it doesn't really matter.
 *
 * Typical instructions for loading data from tape look like this:
 *  - Type "LOAD" or "xxxx.xxxxR", but don't hit <return>.
 *  - Play tape until you here the tone.
 *  - Immediately hit stop.
 *  - Plug the cable from the Apple II into the tape player.
 *  - Hit "play" on the recorder, then immediately hit <return>.
 *  - When the Apple II beeps, it's done.  Stop the tape.
 *
 * How quickly do we need to sample?  The highest frequency we expect to
 * find is 2KHz, so anything over 4KHz should be sufficient.  However, we
 * need to be able to resolve the time between zero transitions to some
 * reasonable resolution.  We need to tell the difference between a 650usec
 * half-cycle and a 200usec half-cycle for the start, and 250/500usec for
 * the data section.  Our measurements can comfortably be off by 200 usec
 * with no ill effects on the lead-in, assuming a perfect signal.  (Sampling
 * every 200 usec would be 5Hz.)  The data itself needs to be +/- 125usec
 * for half-cycles, though we can get a little sloppier if we average the
 * error out by combining half-cycles.
 *
 * The signal is less than perfect, sometimes far less, so we need better
 * sampling to avoid magnifying distortions in the signal.  If we sample
 * at 22.05KHz, we could see a 650usec gap as 590, 635, or 680, depending
 * on when we sample and where we think the peaks lie.  We're off by 15usec
 * before we even start.  We can reasonably expect to be off +/- twice the
 * "usecPerSample" value.  At 8KHz, that's +/- 250usec, which isn't
 * acceptable.  At 11KHz we're at +/- 191usec, which is scraping along.
 *
 * We can get mitigate some problems by doing an interpolation of the
 * two points nearest the zero-crossing, which should give us a more
 * accurate fix on the zero point than simply choosing the closest point.
 * This does potentially increase our risk of errors due to noise spikes at
 * points near the zero.  Since we're reading from cassette, any noise spikes
 * are likely to be pretty wide, so averaging the data or interpolating
 * across multiple points isn't likely to help us.
 *
 * Some tapes seem to have a low-frequency distortion that amounts to a DC
 * bias when examining a single sample.  Timing the gaps between zero
 * crossings is therefore not sufficient unless we also correct for the
 * local DC bias.  In some cases the recorder or media was unable to
 * respond quickly enough, and as a result 0s have less amplitude
 * than 1s.  This throws off some simple correction schemes.
 *
 * The easiest approach is to figure out where one cycle starts and stops, and
 * use the timing of the full cycle.  This gets a little ugly because the
 * original output was a square wave, so there's a bit of ringing in the
 * peaks, especially the 1s.  Of course, we have to look at half-cycles
 * initially, because we need to identify the first "short 0" part.  Once
 * we have that, we can use full cycles, which distributes any error over
 * a larger set of samples.
 *
 * In some cases the positive half-cycle is longer than the negative
 * half-cycle (e.g. reliably 33 samples vs. 29 samples at 48KHz, when
 * 31.2 is expected for 650us).  Slight variations can lead to even
 * greater distortion, even though the timing for the full signal is
 * within tolerances.  This means we need to accumulate the timing for
 * a full cycle before making an evaluation, though we still need to
 * examine the half-cycle timing during the lead-in to catch the "short 0".
 *
 * Because of these distortions, 8-bit 8KHz audio is probably not a good
 * idea.  16-bit 22.05KHz sampling is a better choice for tapes that have
 * been sitting around for 25-30 years.
 */
/*
; Monitor ROM dump, with memory locations rearranged for easier reading.

; Increment 16-bit value at 0x3c (A1) and compare it to 16-bit value at
;  0x3e (A2). Returns with carry set if A1 >= A2.
; Requires 26 cycles in common case, 30 cycles in rare case.
FCBA: A5 3C     709  NXTA1    LDA   A1L        ;INCR 2-BYTE A1.
FCBC: C5 3E     710           CMP   A2L
FCBE: A5 3D     711           LDA   A1H        ;  AND COMPARE TO A2
FCC0: E5 3F     712           SBC   A2H
FCC2: E6 3C     713           INC   A1L        ;  (CARRY SET IF >=)
FCC4: D0 02     714           BNE   RTS4B
FCC6: E6 3D     715           INC   A1H
FCC8: 60        716  RTS4B    RTS

; Write data from location in A1L up to location in A2L.
FECD: A9 40     975  WRITE    LDA   #$40
FECF: 20 C9 FC  976           JSR   HEADR      ;WRITE 10-SEC HEADER
; Write loop.  Continue until A1 reaches A2.
FED2: A0 27     977           LDY   #$27
FED4: A2 00     978  WR1      LDX   #$00
FED6: 41 3C     979           EOR   (A1L,X)
FED8: 48        980           PHA
FED9: A1 3C     981           LDA   (A1L,X)
FEDB: 20 ED FE  982           JSR   WRBYTE
FEDE: 20 BA FC  983           JSR   NXTA1
FEE1: A0 1D     984           LDY   #$1D
FEE3: 68        985           PLA
FEE4: 90 EE     986           BCC   WR1
; Write checksum byte, then beep the speaker.
FEE6: A0 22     987           LDY   #$22
FEE8: 20 ED FE  988           JSR   WRBYTE
FEEB: F0 4D     989           BEQ   BELL

; Write one byte (8 bits, or 16 half-cycles).
; On exit, Z-flag is set.
FEED: A2 10     990  WRBYTE   LDX   #$10
FEEF: 0A        991  WRBYT2   ASL
FEF0: 20 D6 FC  992           JSR   WRBIT
FEF3: D0 FA     993           BNE   WRBYT2
FEF5: 60        994           RTS

; Write tape header.  Called by WRITE with A=$40, READ with A=$16.
; On exit, A holds $FF.
; First time through, X is undefined, so we may get slightly less than
;  A*256 half-cycles (i.e. A*255 + X).  If the carry is clear on entry,
;  the first ADC will subtract two (yielding A*254+X), and the first X
;  cycles will be "long 0s" instead of "long 1s".  Doesn't really matter.
FCC9: A0 4B     717  HEADR    LDY   #$4B       ;WRITE A*256 'LONG 1'
FCCB: 20 DB FC  718           JSR   ZERDLY     ;  HALF CYCLES
FCCE: D0 F9     719           BNE   HEADR      ;  (650 USEC EACH)
FCD0: 69 FE     720           ADC   #$FE
FCD2: B0 F5     721           BCS   HEADR      ;THEN A 'SHORT 0'
; Fall through to write bit.  Note carry is clear, so we'll use the zero
;  delay.  We've initialized Y to $21 instead of $32 to get a short '0'
;  (165usec) for the first half and a normal '0' for the second half;
FCD4: A0 21     722           LDY   #$21       ;  (400 USEC)
; Write one bit.  Called from WRITE with Y=$27.
FCD6: 20 DB FC  723  WRBIT    JSR   ZERDLY     ;WRITE TWO HALF CYCLES
FCD9: C8        724           INY              ;  OF 250 USEC ('0')
FCDA: C8        725           INY              ;  OR 500 USEC ('0')
; Delay for '0'.  X typically holds a bit count or half-cycle count.
; Y holds delay period in 5-usec increments:
;   (carry clear) $21=165us  $27=195us  $2C=220 $4B=375us
;   (carry set) $21=165+250=415us  $27=195+250=445us  $4B=375+250=625us
;   Remember that TOTAL delay, with all other instructions, must equal target
; On exit, Y=$2C, Z-flag is set if X decremented to zero.  The 2C in Y
;  is for WRBYTE, which is in a tight loop and doesn't need much padding.
FCDB: 88        726  ZERDLY   DEY
FCDC: D0 FD     727           BNE   ZERDLY
FCDE: 90 05     728           BCC   WRTAPE     ;Y IS COUNT FOR
; Additional delay for '1' (always 250us).
FCE0: A0 32     729           LDY   #$32       ;  TIMING LOOP
FCE2: 88        730  ONEDLY   DEY
FCE3: D0 FD     731           BNE   ONEDLY
; Write a transition to the tape.
FCE5: AC 20 C0  732  WRTAPE   LDY   TAPEOUT
FCE8: A0 2C     733           LDY   #$2C
FCEA: CA        734           DEX
FCEB: 60        735           RTS

; Read data from location in A1L up to location in A2L.
FEFD: 20 FA FC  999  READ     JSR   RD2BIT     ;FIND TAPEIN EDGE
FF00: A9 16     1000          LDA   #$16
FF02: 20 C9 FC  1001          JSR   HEADR      ;DELAY 3.5 SECONDS
FF05: 85 2E     1002          STA   CHKSUM     ;INIT CHKSUM=$FF
FF07: 20 FA FC  1003          JSR   RD2BIT     ;FIND TAPEIN EDGE
; Loop, waiting for edge.  11 cycles/iteration, plus 432+14 = 457usec.
FF0A: A0 24     1004 RD2      LDY   #$24       ;LOOK FOR SYNC BIT
FF0C: 20 FD FC  1005          JSR   RDBIT      ;  (SHORT 0)
FF0F: B0 F9     1006          BCS   RD2        ;  LOOP UNTIL FOUND
; Timing of next transition, a normal '0' half-cycle, doesn't matter.
FF11: 20 FD FC  1007          JSR   RDBIT      ;SKIP SECOND SYNC H-CYCLE
; Main byte read loop.  Continue until A1 reaches A2.
FF14: A0 3B     1008          LDY   #$3B       ;INDEX FOR 0/1 TEST
FF16: 20 EC FC  1009 RD3      JSR   RDBYTE     ;READ A BYTE
FF19: 81 3C     1010          STA   (A1L,X)    ;STORE AT (A1)
FF1B: 45 2E     1011          EOR   CHKSUM
FF1D: 85 2E     1012          STA   CHKSUM     ;UPDATE RUNNING CHKSUM
FF1F: 20 BA FC  1013          JSR   NXTA1      ;INC A1, COMPARE TO A2
FF22: A0 35     1014          LDY   #$35       ;COMPENSATE 0/1 INDEX
FF24: 90 F0     1015          BCC   RD3        ;LOOP UNTIL DONE
; Read checksum byte and check it.
FF26: 20 EC FC  1016          JSR   RDBYTE     ;READ CHKSUM BYTE
FF29: C5 2E     1017          CMP   CHKSUM
FF2B: F0 0D     1018          BEQ   BELL       ;GOOD, SOUND BELL AND RETURN

; Print "ERR", beep speaker.
FF2D: A9 C5     1019 PRERR    LDA   #$C5
FF2F: 20 ED FD  1020          JSR   COUT       ;PRINT "ERR", THEN BELL
FF32: A9 D2     1021          LDA   #$D2
FF34: 20 ED FD  1022          JSR   COUT
FF37: 20 ED FD  1023          JSR   COUT
FF3A: A9 87     1024 BELL     LDA   #$87       ;OUTPUT BELL AND RETURN
FF3C: 4C ED FD  1025          JMP   COUT

; Read a byte from the tape.  Y is $3B on first call, $35 on subsequent
;  calls.  The bits are shifted left, meaning that the high bit is read
;  first.
FCEC: A2 08     736  RDBYTE   LDX   #$08       ;8 BITS TO READ
FCEE: 48        737  RDBYT2   PHA              ;READ TWO TRANSITIONS
FCEF: 20 FA FC  738           JSR   RD2BIT     ;  (FIND EDGE)
FCF2: 68        739           PLA
FCF3: 2A        740           ROL              ;NEXT BIT
FCF4: A0 3A     741           LDY   #$3A       ;COUNT FOR SAMPLES
FCF6: CA        742           DEX
FCF7: D0 F5     743           BNE   RDBYT2
FCF9: 60        744           RTS

; Read two bits from the tape.
FCFA: 20 FD FC  745  RD2BIT   JSR   RDBIT
; Read one bit from the tape.  On entry, Y is the expected transition time:
;   $3A=696usec  $35=636usec  $24=432usec
; Returns with the carry set if the transition time exceeds the Y value.
FCFD: 88        746  RDBIT    DEY              ;DECR Y UNTIL
FCFE: AD 60 C0  747           LDA   TAPEIN     ; TAPE TRANSITION
FD01: 45 2F     748           EOR   LASTIN
FD03: 10 F8     749           BPL   RDBIT
; the above loop takes 12 usec per iteration, what follows takes 14.
FD05: 45 2F     750           EOR   LASTIN
FD07: 85 2F     751           STA   LASTIN
FD09: C0 80     752           CPY   #$80       ;SET CARRY ON Y
FD0B: 60        753           RTS

*/


/*
 * ==========================================================================
 *      CassetteDialog
 * ==========================================================================
 */

BEGIN_MESSAGE_MAP(CassetteDialog, CDialog)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_CASSETTE_LIST, OnListChange)
    ON_NOTIFY(NM_DBLCLK, IDC_CASSETTE_LIST, OnListDblClick)
    //ON_MESSAGE(WMU_DIALOG_READY, OnDialogReady)
    ON_COMMAND(IDC_IMPORT_CHUNK, OnImport)
    ON_COMMAND(IDHELP, OnHelp)
    ON_CBN_SELCHANGE(IDC_CASSETTE_ALG, OnAlgorithmChange)
END_MESSAGE_MAP()


BOOL CassetteDialog::OnInitDialog(void)
{
    CRect rect;
    const Preferences* pPreferences = GET_PREFERENCES();

    CDialog::OnInitDialog();        // does DDX init

    CWnd* pWnd;
    pWnd = GetDlgItem(IDC_IMPORT_CHUNK);
    pWnd->EnableWindow(FALSE);

    pWnd = GetDlgItem(IDC_CASSETTE_INPUT);
    pWnd->SetWindowText(fFileName);

    /* prep the combo box */
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_CASSETTE_ALG);
    ASSERT(pCombo != NULL);
    int defaultAlg = pPreferences->GetPrefLong(kPrCassetteAlgorithm);
    if (defaultAlg > CassetteData::kAlgorithmMIN &&
        defaultAlg < CassetteData::kAlgorithmMAX)
    {
        pCombo->SetCurSel(defaultAlg);
    } else {
        LOGI("GLITCH: invalid defaultAlg in prefs (%d)", defaultAlg);
        pCombo->SetCurSel(CassetteData::kAlgorithmZero);
    }
    fAlgorithm = (CassetteData::Algorithm) defaultAlg;

    /*
     * Prep the listview control.
     *
     * Columns:
     *  [icon] Index | Format | Length | Checksum OK
     */
    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_CASSETTE_LIST);
    ASSERT(pListView != NULL);
    ListView_SetExtendedListViewStyleEx(pListView->m_hWnd,
        LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

    int width0, width1, width2, width3, width4;

    pListView->GetClientRect(&rect);
    width0 = pListView->GetStringWidth(L"XXIndexX");
    width1 = pListView->GetStringWidth(L"XXFormatXmmmmmmmmmmmmmm");
    width2 = pListView->GetStringWidth(L"XXLengthXm");
    width3 = pListView->GetStringWidth(L"XXChecksumXm");
    width4 = pListView->GetStringWidth(L"XXStart sampleX");
    //width5 = pListView->GetStringWidth("XXEnd sampleX");

    pListView->InsertColumn(0, L"Index", LVCFMT_LEFT, width0);
    pListView->InsertColumn(1, L"Format", LVCFMT_LEFT, width1);
    pListView->InsertColumn(2, L"Length", LVCFMT_LEFT, width2);
    pListView->InsertColumn(3, L"Checksum", LVCFMT_LEFT, width3);
    pListView->InsertColumn(4, L"Start sample", LVCFMT_LEFT, width4);
    pListView->InsertColumn(5, L"End sample", LVCFMT_LEFT,
        rect.Width() - (width0+width1+width2+width3+width4)
        /*- ::GetSystemMetrics(SM_CXVSCROLL)*/ );

    /* add images for list; this MUST be loaded before header images */
//  LoadListImages();
//  pListView->SetImageList(&fListImageList, LVSIL_SMALL);

//  LoadList();

    CenterWindow();

    //int cc = PostMessage(WMU_DIALOG_READY, 0, 0);
    //ASSERT(cc != 0);

    if (!AnalyzeWAV())
        OnCancel();

    return TRUE;
}

#if 0
/*
 * Dialog construction has completed.  Start the WAV analysis.
 */
LONG
CassetteDialog::OnDialogReady(UINT, LONG)
{
    //AnalyzeWAV();
    return 0;
}
#endif


void CassetteDialog::OnListChange(NMHDR*, LRESULT* pResult)
{
    LOGI("List change");
    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_CASSETTE_LIST);
    CButton* pButton = (CButton*) GetDlgItem(IDC_IMPORT_CHUNK);
    pButton->EnableWindow(pListView->GetSelectedCount() != 0);

    *pResult = 0;
}


void CassetteDialog::OnListDblClick(NMHDR* pNotifyStruct, LRESULT* pResult)
{
    LOGI("Double click!");
    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_CASSETTE_LIST);

    if (pListView->GetSelectedCount() == 1)
        OnImport();

    *pResult = 0;
}

void CassetteDialog::OnAlgorithmChange(void)
{
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_CASSETTE_ALG);
    ASSERT(pCombo != NULL);
    LOGI("+++ SELECTION IS NOW %d", pCombo->GetCurSel());
    fAlgorithm = (CassetteData::Algorithm) pCombo->GetCurSel();
    AnalyzeWAV();
}

void CassetteDialog::OnImport(void)
{
    /*
     * Figure out which item they have selected.
     */
    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_CASSETTE_LIST);
    ASSERT(pListView != NULL);
    assert(pListView->GetSelectedCount() == 1);

    POSITION posn;
    posn = pListView->GetFirstSelectedItemPosition();
    if (posn == NULL) {
        ASSERT(false);
        return;
    }
    int idx = pListView->GetNextSelectedItem(posn);

    /*
     * Set up the import dialog.
     */
    CassImpTargetDialog impDialog(this);

    impDialog.fFileName = "From.Tape";
    impDialog.fFileLength = fDataArray[idx].GetDataLen();
    impDialog.SetFileType(fDataArray[idx].GetFileType());

    if (impDialog.DoModal() != IDOK)
        return;

    /*
     * Write the file to the currently-open archive.
     */
    GenericArchive::LocalFileDetails details;

    details.SetEntryKind(GenericArchive::LocalFileDetails::kFileKindDataFork);
    details.SetLocalPathName(L"Cassette WAV");
    details.SetStrippedLocalPathName(impDialog.fFileName);
    details.SetAccess(0xe3);    // unlocked, backup bit set
    details.SetFileType(impDialog.GetFileType());
    if (details.GetFileType() == kFileTypeBIN) {
        details.SetExtraType(impDialog.fStartAddr);
    } else if (details.GetFileType() == kFileTypeBAS) {
        details.SetExtraType(0x0801);
    } else {
        details.SetExtraType(0x0000);
    }
    details.SetStorageType(DiskFS::kStorageSeedling);
    time_t now = time(NULL);
    NuDateTime ndt;
    GenericArchive::UNIXTimeToDateTime(&now, &ndt);
    details.SetCreateWhen(ndt);
    details.SetArchiveWhen(ndt);
    details.SetModWhen(ndt);

    CString errMsg;

    fDirty = true;
    if (!MainWindow::SaveToArchive(&details, fDataArray[idx].GetDataBuf(),
        fDataArray[idx].GetDataLen(), NULL, -1, &errMsg, this))
    {
        goto bail;
    }


bail:
    if (!errMsg.IsEmpty()) {
        CString msg;
        msg.Format(L"Unable to import file: %ls.", (LPCWSTR) errMsg);
        ShowFailureMsg(this, msg, IDS_FAILED);
        return;
    }
}

bool CassetteDialog::AnalyzeWAV(void)
{
    SoundFile soundFile;
    CWaitCursor waitc;
    CListCtrl* pListCtrl = (CListCtrl*) GetDlgItem(IDC_CASSETTE_LIST);
    CString errMsg;
    long sampleOffset;
    int idx;

    if (soundFile.Create(fFileName, &errMsg) != 0) {
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        return false;
    }

    const WAVEFORMATEX* pFormat = soundFile.GetWaveFormat();
    if (pFormat->nChannels < 1 || pFormat->nChannels > 2 ||
        (pFormat->wBitsPerSample != 8 && pFormat->wBitsPerSample != 16))
    {
        errMsg.Format(L"Unexpected PCM format (%d channels, %d bits/sample)",
            pFormat->nChannels, pFormat->wBitsPerSample);
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        return false;
    }
    if (soundFile.GetDataLen() % soundFile.GetBPS() != 0) {
        errMsg.Format(L"Unexpected sound data length (%ld, samples are %d bytes)",
            soundFile.GetDataLen(), soundFile.GetBPS());
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        return false;
    }

    pListCtrl->DeleteAllItems();

    sampleOffset = 0;
    for (idx = 0; idx < kMaxRecordings; idx++) {
        long fileType;
        bool result;

        result = fDataArray[idx].Scan(&soundFile, fAlgorithm, &sampleOffset);
        if (!result)
            break;

        AddEntry(idx, pListCtrl, &fileType);
        fDataArray[idx].SetFileType(fileType);
    }

    if (idx == 0) {
        LOGI("No Apple II files found");
        /* that's okay, just show the empty list */
    }

    return true;
}

void CassetteDialog::AddEntry(int idx, CListCtrl* pListCtrl, long* pFileType)
{
    CString tmpStr;
    const CassetteData* pData = &fDataArray[idx];
    const unsigned char* pDataBuf = pData->GetDataBuf();

    ASSERT(pDataBuf != NULL);

    tmpStr.Format(L"%d", idx);
    pListCtrl->InsertItem(idx, tmpStr);

    *pFileType = kFileTypeBIN;
    if (pData->GetDataLen() == 2) {
        tmpStr.Format(L"Integer header ($%04X)",
            pDataBuf[0] | pDataBuf[1] << 8);
    } else if (pData->GetDataLen() == 3) {
        tmpStr.Format(L"Applesoft header ($%04X $%02x)",
            pDataBuf[0] | pDataBuf[1] << 8, pDataBuf[2]);
    } else if (pData->GetDataLen() > 3 && idx > 0 &&
        fDataArray[idx-1].GetDataLen() == 2)
    {
        tmpStr = L"Integer BASIC";
        *pFileType = kFileTypeINT;
    } else if (pData->GetDataLen() > 3 && idx > 0 &&
        fDataArray[idx-1].GetDataLen() == 3)
    {
        tmpStr = L"Applesoft BASIC";
        *pFileType = kFileTypeBAS;
    } else {
        tmpStr = L"Binary";
    }
    pListCtrl->SetItemText(idx, 1, tmpStr);
    
    tmpStr.Format(L"%d", pData->GetDataLen());
    pListCtrl->SetItemText(idx, 2, tmpStr);
    if (pData->GetDataChkGood())
        tmpStr.Format(L"Good (0x%02x)", pData->GetDataChecksum());
    else
        tmpStr.Format(L"BAD (0x%02x)", pData->GetDataChecksum());
    pListCtrl->SetItemText(idx, 3, tmpStr);
    tmpStr.Format(L"%ld", pData->GetDataOffset());
    pListCtrl->SetItemText(idx, 4, tmpStr);
    tmpStr.Format(L"%ld", pData->GetDataEndOffset());
    pListCtrl->SetItemText(idx, 5, tmpStr);
}


/*
 * ==========================================================================
 *      CassetteData
 * ==========================================================================
 */

bool CassetteDialog::CassetteData::Scan(SoundFile* pSoundFile, Algorithm alg,
    long* pStartOffset)
{
    const int kSampleChunkSize = 65536;     // should be multiple of 4
    const WAVEFORMATEX* pFormat;
    ScanState scanState;
    long initialLen, dataLen, chunkLen, byteOffset;
    long sampleStartIndex;
    unsigned char* buf = NULL;
    float* sampleBuf = NULL;
    int bytesPerSample;
    bool result = false;
    unsigned char checkSum;
    int outByteIndex, bitAcc;

    bytesPerSample = pSoundFile->GetBPS();
    assert(bytesPerSample >= 1 && bytesPerSample <= 4);
    assert(kSampleChunkSize % bytesPerSample == 0);
    byteOffset = *pStartOffset;
    initialLen = dataLen = pSoundFile->GetDataLen() - byteOffset;
    sampleStartIndex = byteOffset/bytesPerSample;
    LOGI("CassetteData::Scan(off=%ld / %ld) len=%ld  alg=%d",
        byteOffset, sampleStartIndex, dataLen, alg);

    pFormat = pSoundFile->GetWaveFormat();

    buf = new unsigned char[kSampleChunkSize];
    sampleBuf = new float[kSampleChunkSize/bytesPerSample];
    if (fOutputBuf == NULL)  // alloc on first use
        fOutputBuf = new unsigned char[kMaxFileLen];
    if (buf == NULL || sampleBuf == NULL || fOutputBuf == NULL) {
        LOGI("Buffer alloc failed");
        goto bail;
    }

    memset(&scanState, 0, sizeof(scanState));
    scanState.algorithm = alg;
    scanState.phase = kPhaseScanFor770Start;
    scanState.mode = kModeInitial0;
    scanState.positive = false;
    scanState.usecPerSample = 1000000.0f / (float) pFormat->nSamplesPerSec;

    checkSum = 0xff;
    outByteIndex = 0;
    bitAcc = 1;

    /*
     * Loop until done or out of data.
     */
    while (dataLen > 0) {
        int cc;

        chunkLen = dataLen;
        if (chunkLen > kSampleChunkSize)
            chunkLen = kSampleChunkSize;

        cc = pSoundFile->ReadData(buf, byteOffset, chunkLen);
        if (cc < 0) {
            LOGI("ReadData(%d) failed", chunkLen);
            goto bail;
        }

        ConvertSamplesToReal(pFormat, buf, chunkLen, sampleBuf);

        for (int i = 0; i < chunkLen / bytesPerSample; i++) {
            int bitVal;
            if (ProcessSample(sampleBuf[i], sampleStartIndex + i,
                &scanState, &bitVal))
            {
                if (outByteIndex >= kMaxFileLen) {
                    LOGI("Cassette data overflow");
                    scanState.phase = kPhaseEndReached;
                } else {
                    /* output a bit, shifting until bit 8 lights up */
                    assert(bitVal == 0 || bitVal == 1);
                    bitAcc = (bitAcc << 1) | bitVal;
                    if (bitAcc > 0xff) {
                        fOutputBuf[outByteIndex++] = (unsigned char) bitAcc;
                        checkSum ^= (unsigned char) bitAcc;
                        bitAcc = 1;
                    }
                }
            }
            if (scanState.phase == kPhaseEndReached) {
                dataLen -= i * bytesPerSample;
                break;
            }
        }
        if (scanState.phase == kPhaseEndReached)
            break;

        dataLen -= chunkLen;
        byteOffset += chunkLen;
        sampleStartIndex += chunkLen / bytesPerSample;
    }

    switch (scanState.phase) {
    case kPhaseScanFor770Start:
    case kPhaseScanning770:
        // expected case for trailing part of file
        LOGI("Scan ended while searching for 770");
        goto bail;
    case kPhaseScanForShort0:
    case kPhaseShort0B:
        LOGI("Scan ended while searching for short 0/0B");
        //DebugBreak(); // unusual
        goto bail;
    case kPhaseReadData:
        LOGI("Scan ended while reading data");
        //DebugBreak(); // truncated WAV file?
        goto bail;
    case kPhaseEndReached:
        LOGI("Scan found end");
        // winner!
        break;
    default:
        LOGI("Unknown phase %d", scanState.phase);
        assert(false);
        goto bail;
    }

    LOGI("*** Output %d bytes (bitAcc=0x%02x, checkSum=0x%02x)",
        outByteIndex, bitAcc, checkSum);

    if (outByteIndex == 0) {
        fOutputLen = 0;
        fChecksum = 0x00;
        fChecksumGood = false;
    } else {
        fOutputLen = outByteIndex-1;
        fChecksum = fOutputBuf[outByteIndex-1];
        fChecksumGood = (checkSum == 0x00);
    }
    fStartSample = scanState.dataStart;
    fEndSample = scanState.dataEnd;

    /* we're done with this file; advance the start offset */
    *pStartOffset = *pStartOffset + (initialLen - dataLen);

    result = true;

bail:
    delete[] buf;
    delete[] sampleBuf;
    return result;
}

void CassetteDialog::CassetteData::ConvertSamplesToReal(const WAVEFORMATEX* pFormat,
    const unsigned char* buf, long chunkLen, float* sampleBuf)
{
    int bps = ((pFormat->wBitsPerSample+7)/8) * pFormat->nChannels;
    int bitsPerSample = pFormat->wBitsPerSample;
    int offset = 0;

    assert(chunkLen % bps == 0);

    if (bitsPerSample == 8) {
        while (chunkLen > 0) {
            *sampleBuf++ = (*buf - 128) / 128.0f;
            //LOGI("Sample8(%5d)=%d float=%.3f", offset, *buf, *(sampleBuf-1));
            //offset++;
            buf += bps;
            chunkLen -= bps;
        }
    } else if (bitsPerSample == 16) {
        while (chunkLen > 0) {
            short sample = *buf | *(buf+1) << 8;
            *sampleBuf++ = sample / 32768.0f;
            //LOGI("Sample16(%5d)=%d float=%.3f", offset, sample, *(sampleBuf-1));
            //offset++;
            buf += bps;
            chunkLen -= bps;
        }
    } else {
        assert(false);
    }

    //LOGI("Conv %d", bitsPerSample);
}

/* width of 1/2 cycle in 770Hz lead-in */
const float kLeadInHalfWidth = 650.0f;      // usec
/* max error when detecting 770Hz lead-in, in usec */
const float kLeadInMaxError = 108.0f;       // usec (542 - 758)
/* width of 1/2 cycle of "short 0" */
const float kShortZeroHalfWidth = 200.0f;   // usec
/* max error when detection short 0 */
const float kShortZeroMaxError = 150.0f;    // usec (50 - 350)
/* width of 1/2 cycle of '0' */
const float kZeroHalfWidth = 250.0f;        // usec
/* max error when detecting '0' */
const float kZeroMaxError = 94.0f;          // usec
/* width of 1/2 cycle of '1' */
const float kOneHalfWidth = 500.0f;         // usec
/* max error when detecting '1' */
const float kOneMaxError = 94.0f;           // usec
/* after this many 770Hz half-cycles, start looking for short 0 */
const long kLeadInHalfCycThreshold = 1540;  // 1 full second

/* amplitude must change by this much before we switch out of "peak" mode */
const float kPeakThreshold = 0.2f;          // 10%
/* amplitude must change by at least this much to stay in "transition" mode */
const float kTransMinDelta = 0.02f;         // 1%
/* kTransMinDelta happens over this range */
const float kTransDeltaBase = 45.35f;       // usec (1 sample at 22.05KHz)


bool CassetteDialog::CassetteData::ProcessSample(float sample, long sampleIndex,
    ScanState* pScanState, int* pBitVal)
{
    if (pScanState->algorithm == kAlgorithmZero)
        return ProcessSampleZero(sample, sampleIndex, pScanState, pBitVal);
    else if (pScanState->algorithm == kAlgorithmRoundPeak ||
             pScanState->algorithm == kAlgorithmSharpPeak ||
             pScanState->algorithm == kAlgorithmShallowPeak)
        return ProcessSamplePeak(sample, sampleIndex, pScanState, pBitVal);
    else {
        assert(false);
        return false;
    }
}

bool CassetteDialog::CassetteData::ProcessSampleZero(float sample, long sampleIndex,
    ScanState* pScanState, int* pBitVal)
{
    long timeDelta;
    bool crossedZero = false;
    bool emitBit = false;

    /*
     * Analyze the mode, changing to a new one when appropriate.
     */
    switch (pScanState->mode) {
    case kModeInitial0:
        assert(pScanState->phase == kPhaseScanFor770Start);
        pScanState->mode = kModeRunning;
        break;
    case kModeRunning:
        if (pScanState->prevSample < 0.0f && sample >= 0.0f ||
            pScanState->prevSample >= 0.0f && sample < 0.0f)
        {
            crossedZero = true;
        }
        break;
    default:
        assert(false);
        break;
    }

    /*
     * Deal with a zero crossing.
     *
     * We currently just grab the first point after we cross.  We should
     * be grabbing the closest point or interpolating across.
     */
    if (crossedZero) {
        float halfCycleUsec;
        int bias;

        if (fabs(pScanState->prevSample) < fabs(sample))
            bias = -1;      // previous sample was closer to zero point
        else
            bias = 0;       // current sample is closer

        /* delta time for zero-to-zero (half cycle) */
        timeDelta = (sampleIndex+bias) - pScanState->lastZeroIndex;

        halfCycleUsec = timeDelta * pScanState->usecPerSample;
        //LOGI("Zero %6ld: half=%.1fusec full=%.1fusec",
        //  sampleIndex, halfCycleUsec,
        //  halfCycleUsec + pScanState->halfCycleWidth);

        emitBit = UpdatePhase(pScanState, sampleIndex+bias, halfCycleUsec,
            pBitVal);

        pScanState->lastZeroIndex = sampleIndex + bias;
    }

    /* record this sample for the next go-round */
    pScanState->prevSample = sample;

    return emitBit;
}

bool CassetteDialog::CassetteData::ProcessSamplePeak(float sample, long sampleIndex,
    ScanState* pScanState, int* pBitVal)
{
    /* values range from [-1.0,1.0), so range is 2.0 total */
    long timeDelta;
    float ampDelta;
    float transitionLimit;
    bool hitPeak = false;
    bool emitBit = false;

    /*
     * Analyze the mode, changing to a new one when appropriate.
     */
    switch (pScanState->mode) {
    case kModeInitial0:
        assert(pScanState->phase == kPhaseScanFor770Start);
        pScanState->mode = kModeInitial1;
        break;
    case kModeInitial1:
        assert(pScanState->phase == kPhaseScanFor770Start);
        if (sample >= pScanState->prevSample)
            pScanState->positive = true;
        else
            pScanState->positive = false;
        pScanState->mode = kModeInTransition;
        /* set these up with something reasonable */
        pScanState->lastPeakStartIndex = sampleIndex;
        pScanState->lastPeakStartValue = sample;
        break;

    case kModeInTransition:
        /*
         * Stay here until two adjacent samples are very close in amplitude
         * (or we change direction).  We need to adjust our amplitude
         * threshold based on sampling frequency, or at higher sample
         * rates we're going to think everything is a transition.
         *
         * The approach here is overly simplistic, and is prone to failure
         * when the sampling rate is high, especially with 8-bit samples
         * or sound cards that don't really have 16-bit resolution.  The
         * proper way to do this is to keep a short history, and evaluate
         * the delta amplitude over longer periods.  [At this point I'd
         * rather just tell people to record at 22.05KHz.]
         *
         * Set the "hitPeak" flag and handle the consequences below.
         */
        if (pScanState->algorithm == kAlgorithmRoundPeak)
            transitionLimit = kTransMinDelta *
                    (pScanState->usecPerSample / kTransDeltaBase);
        else
            transitionLimit = 0.0f;

        if (pScanState->positive) {
            if (sample < pScanState->prevSample + transitionLimit) {
                pScanState->mode = kModeAtPeak;
                hitPeak = true;
            }
        } else {
            if (sample > pScanState->prevSample - transitionLimit) {
                pScanState->mode = kModeAtPeak;
                hitPeak = true;
            }
        }
        break;
    case kModeAtPeak:
        /*
         * Stay here until we're a certain distance above or below the
         * previous peak.  This also keeps us in a holding pattern for
         * large flat areas.
         */
        transitionLimit = kPeakThreshold;
        if (pScanState->algorithm == kAlgorithmShallowPeak)
            transitionLimit /= 4.0f;

        ampDelta = pScanState->lastPeakStartValue - sample;
        if (ampDelta < 0)
            ampDelta = -ampDelta;
        if (ampDelta > transitionLimit) {
            if (sample >= pScanState->lastPeakStartValue)
                pScanState->positive = true;        // going up
            else
                pScanState->positive = false;       // going down

            /* mark the end of the peak; could be same as start of peak */
            pScanState->mode = kModeInTransition;
        }
        break;
    default:
        assert(false);
        break;
    }

    /*
     * If we hit "peak" criteria, we regard the *previous* sample as the
     * peak.  This is very important for lower sampling rates (e.g. 8KHz).
     */
    if (hitPeak) {
        /* compute half-cycle amplitude and time */
        float halfCycleUsec; //, fullCycleUsec;

        /* delta time for peak-to-peak (half cycle) */
        timeDelta = (sampleIndex-1) - pScanState->lastPeakStartIndex;
        /* amplitude peak-to-peak */
        ampDelta = pScanState->lastPeakStartValue - pScanState->prevSample;
        if (ampDelta < 0)
            ampDelta = -ampDelta;

        halfCycleUsec = timeDelta * pScanState->usecPerSample;
        //if (sampleIndex > 584327 && sampleIndex < 590000) {
        //  LOGI("Peak %6ld: amp=%.3f height=%.3f peakWidth=%.1fusec",
        //      sampleIndex-1, pScanState->prevSample, ampDelta,
        //      halfCycleUsec);
        //  ::Sleep(10);
        //}
        if (sampleIndex == 32739)
            LOGI("whee");

        emitBit = UpdatePhase(pScanState, sampleIndex-1, halfCycleUsec, pBitVal);

        /* set the "peak start" values */
        pScanState->lastPeakStartIndex = sampleIndex-1;
        pScanState->lastPeakStartValue = pScanState->prevSample;
    }

    /* record this sample for the next go-round */
    pScanState->prevSample = sample;

    return emitBit;
}

bool CassetteDialog::CassetteData::UpdatePhase(ScanState* pScanState,
    long sampleIndex, float halfCycleUsec, int* pBitVal)
{
    float fullCycleUsec;
    bool emitBit = false;

    if (pScanState->halfCycleWidth != 0.0f)
        fullCycleUsec = halfCycleUsec + pScanState->halfCycleWidth;
    else
        fullCycleUsec = 0.0f;   // only have first half

    switch (pScanState->phase) {
    case kPhaseScanFor770Start:
        /* watch for a cycle of the appropriate length */
        if (fullCycleUsec != 0.0f &&
            fullCycleUsec > kLeadInHalfWidth*2.0f - kLeadInMaxError*2.0f &&
            fullCycleUsec < kLeadInHalfWidth*2.0f + kLeadInMaxError*2.0f)
        {
            //LOGI("  scanning 770 at %ld", sampleIndex);
            pScanState->phase = kPhaseScanning770;
            pScanState->num770 = 1;
        }
        break;
    case kPhaseScanning770:
        /* count up the 770Hz cycles */
        if (fullCycleUsec != 0.0f &&
            fullCycleUsec > kLeadInHalfWidth*2.0f - kLeadInMaxError*2.0f &&
            fullCycleUsec < kLeadInHalfWidth*2.0f + kLeadInMaxError*2.0f)
        {
            pScanState->num770++;
            if (pScanState->num770 > kLeadInHalfCycThreshold/2) {
                /* looks like a solid tone, advance to next phase */
                pScanState->phase = kPhaseScanForShort0;
                LOGI("  looking for short 0");
            }
        } else if (fullCycleUsec != 0.0f) {
            /* pattern lost, reset */
            if (pScanState->num770 > 5) {
                LOGI("  lost 770 at %ld width=%.1f (count=%ld)",
                    sampleIndex, fullCycleUsec, pScanState->num770);
            }
            pScanState->phase = kPhaseScanFor770Start;
        }
        /* else we only have a half cycle, so do nothing */
        break;
    case kPhaseScanForShort0:
        /* found what looks like a 770Hz field, find the short 0 */
        if (halfCycleUsec > kShortZeroHalfWidth - kShortZeroMaxError &&
            halfCycleUsec < kShortZeroHalfWidth + kShortZeroMaxError)
        {
            LOGI("  found short zero (half=%.1f) at %ld after %ld 770s",
                halfCycleUsec, sampleIndex, pScanState->num770);
            pScanState->phase = kPhaseShort0B;
            /* make sure we treat current sample as first half */
            pScanState->halfCycleWidth = 0.0f;
        } else
        if (fullCycleUsec != 0.0f &&
            fullCycleUsec > kLeadInHalfWidth*2.0f - kLeadInMaxError*2.0f &&
            fullCycleUsec < kLeadInHalfWidth*2.0f + kLeadInMaxError*2.0f)
        {
            /* found another 770Hz cycle */
            pScanState->num770++;
        } else if (fullCycleUsec != 0.0f) {
            /* full cycle of the wrong size, we've lost it */
            LOGI("  Lost 770 at %ld width=%.1f (count=%ld)",
                sampleIndex, fullCycleUsec, pScanState->num770);
            pScanState->phase = kPhaseScanFor770Start;
        }
        break;
    case kPhaseShort0B:
        /* pick up the second half of the start cycle */
        assert(fullCycleUsec != 0.0f);
        if (fullCycleUsec > (kShortZeroHalfWidth + kZeroHalfWidth) - kZeroMaxError*2.0f &&
            fullCycleUsec < (kShortZeroHalfWidth + kZeroHalfWidth) + kZeroMaxError*2.0f)
        {
            /* as expected */
            LOGI("  Found 0B %.1f (total %.1f), advancing to 'read data' phase",
                halfCycleUsec, fullCycleUsec);
            pScanState->dataStart = sampleIndex;
            pScanState->phase = kPhaseReadData;
        } else {
            /* must be a false-positive at end of tone */
            LOGI("  Didn't find post-short-0 value (half=%.1f + %.1f)",
                pScanState->halfCycleWidth, halfCycleUsec);
            pScanState->phase = kPhaseScanFor770Start;
        }
        break;

    case kPhaseReadData:
        /* check width of full cycle; don't double error allowance */
        if (fullCycleUsec != 0.0f) {
            if (fullCycleUsec > kZeroHalfWidth*2 - kZeroMaxError*2 &&
                fullCycleUsec < kZeroHalfWidth*2 + kZeroMaxError*2)
            {
                *pBitVal = 0;
                emitBit = true;
            } else
            if (fullCycleUsec > kOneHalfWidth*2 - kOneMaxError*2 &&
                fullCycleUsec < kOneHalfWidth*2 + kOneMaxError*2)
            {
                *pBitVal = 1;
                emitBit = true;
            } else {
                /* bad cycle, assume end reached */
                LOGI("  Bad full cycle time %.1f in data at %ld, bailing",
                    fullCycleUsec, sampleIndex);
                pScanState->dataEnd = sampleIndex;
                pScanState->phase = kPhaseEndReached;
            }
        }
        break;
    default:
        assert(false);
        break;
    }

    /* save the half-cycle stats */
    if (pScanState->halfCycleWidth == 0.0f)
        pScanState->halfCycleWidth = halfCycleUsec;
    else
        pScanState->halfCycleWidth = 0.0f;

    return emitBit;
}
