/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat assembly-language source code.
 */
#ifndef REFORMAT_ASM_H
#define REFORMAT_ASM_H

#include "ReformatBase.h"

/*
 * Base class for assembly source conversion.
 *
 * We want to tab to certain offsets, either hard coded or found in the source
 * file.  To make this work right we want to buffer each line of output.  The
 * maximum line length is bounded by the editor.
 */
class ReformatAsm : public ReformatText {
public:
    ReformatAsm(void) {}
    virtual ~ReformatAsm(void) {}

    void OutputStart(void) {
        fOutBufIndex = 0;
        memset(fOutBuf, ' ', kMaxLineLen);
    }
    void OutputFinish(void) {
        Output('\0');
    }

    void Output(char ch) {
        if (fOutBufIndex < kMaxLineLen)
            fOutBuf[fOutBufIndex++] = ch;
    }
    void Output(const char* str) {
        int len = strlen(str);
        if (fOutBufIndex + len > kMaxLineLen)
            len = kMaxLineLen - fOutBufIndex;
        if (len > 0) {
            memcpy(fOutBuf + fOutBufIndex, str, len);
            fOutBufIndex += len;
        }
    }
    void OutputTab(int posn) {
        if (posn < kMaxLineLen && posn > fOutBufIndex)
            fOutBufIndex = posn;
        else
            Output(' ');        // always at least one
    }

    const char* GetOutBuf(void) const { return fOutBuf; }

private:
    enum {
        kMaxLineLen = 128,
    };

    char    fOutBuf[kMaxLineLen+1];
    int     fOutBufIndex;
};


/*
 * Reformat an S-C Assembler listing into readable text.
 */
class ReformatSCAssem : public ReformatText {
public:
    ReformatSCAssem(void) {}
    virtual ~ReformatSCAssem(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

    /* this gets called by Integer BASIC converter */
    static bool IsSCAssem(const ReformatHolder* pHolder);
};


/*
 * Reformat Merlin listing to have tabs.
 */
class ReformatMerlin : public ReformatAsm {
public:
    ReformatMerlin(void) {}
    virtual ~ReformatMerlin(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
    static bool IsMerlin(const ReformatHolder* pHolder);
};


/*
 * Reformat LISA v2 (DOS) sources.
 */
class ReformatLISA2 : public ReformatAsm {
public:
    ReformatLISA2(void) {}
    virtual ~ReformatLISA2(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
    bool IsLISA(const ReformatHolder* pHolder);

    enum {
        kOpTab = 9,
        kAdTab = 13,
        kComTab = 39,
    };

    void ProcessLine(const uint8_t* buf);
};

/*
 * Reformat LISA v3 (ProDOS) sources.
 */
class ReformatLISA3 : public ReformatAsm {
public:
    ReformatLISA3(void) : fSymTab(NULL) {}
    virtual ~ReformatLISA3(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

    static bool IsLISA(const ReformatHolder* pHolder);

private:
    typedef enum OperandResult {
        kResultUnknown = 0,
        kResultFailed,
        kResultGotoOutOprtr,
        kResultGotoOutOprnd,
    } OperandResult;

    void ProcessLine(const uint8_t* codePtr, int len);
    void ConvertOperand(uint8_t mnemonic, const uint8_t** pCodePtr,
        int* pLen);
    OperandResult PrintNum(int adrsMode, uint8_t val,
        const uint8_t** pCodePtr, int* pLen);
    OperandResult PrintComplexOperand(uint8_t opr,
        const uint8_t** pCodePtr, int* pLen);
    void PrintSymEntry(int ent);
    void PrintMnemonic(uint8_t val);
    void PrintBin(uint8_t val);
    void PrintComment(int adrsMode, const uint8_t* ptr, int len);

    enum {
        kHeaderLen = 4,
        kOpTab = 9,
        kAdTab = 18,
        kComTab = 40,
    };
    /* token constants, from LISA.MNEMONICS */
    enum {
        kLCLTKN = 0xF0,
        kLBLTKN = 0xFA,
        // 0xFB used to indicate "second half" of symbol table
        kMACTKN = 0xFC,
        // 0xFD used to indicate "second half" of symbol table
        kCMNTTKN = 0xFE,
        kSN = 0x00,
        kSS = 0xB0,

        kGROUP1 = 0x00,
        kGROUP2 = 0x0b,
        kGROUP3 = 0x11,
        kGROUP4 = 0x24,
        kGROUP5 = 0x29,
        kGROUP6 = 0x34,
        kGROUP7 = 0x39,
        kGROUP8 = 0x3f,
        kGROUP9 = 0x4a,
    };

    const uint8_t* fSymTab;
    uint16_t       fSymCount;

};

/*
 * Reformat LISA v4/v5 (ProDOS and GS/OS) sources.
 */
class ReformatLISA4 : public ReformatAsm {
public:
    ReformatLISA4(void) : fSymTab(NULL) {}
    virtual ~ReformatLISA4(void) { delete[] fSymTab; }

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

    static bool IsLISA(const ReformatHolder* pHolder);

private:
    enum {
        kHeaderLen = 16,
    };

    /* token constants, from MNEMONICS.A */
    enum {
        kBigndec4_tkn = 0x00,
        kBignhex4_tkn = 0x01,
        kBignbin4_tkn = 0x02,
        kBignhexs_tkn = 0x03,
        kBignstring_tkn = 0x04,
        kDec1_tkn = 0x1a,
        kDec2_tkn = 0x1b,
        kHex1_tkn = 0x1c,
        kHex2_tkn = 0x1d,
        kBin1_tkn = 0x1e,
        kBin2_tkn = 0x1f,
        kBign_tkn = 0x33,
        kDec3_tkn = 0x34,
        kHex3_tkn = 0x35,
        kBin3_tkn = 0x36,
        kcABS_tkn = 0x37,
        kcLONG_tkn = 0x38,
        kMVN_tkn = 0x41,
        kMVP_tkn = 0x42,
        kMacE_tkn = 0x5e,
        kStr31_tkn = 0x7f,

        kSS = 0x90,

        //kGROUP1_tkns = 0x01,
        //kGROUP2_tkns = 0x0c,
        kGROUP3_tkns = 0x12,
        //kGROUP4_tkns = 0x29,
        kGROUP5_tkns = 0x30,
        kGROUP6_tkns = 0x43,
        //kGROUP7_tkns = 0x4a,
        //kGROUP8_tkns = 0x51,
        //kGROUP9_tkns = 0x5f,

        kLocalTKN = 0xf0,
        kLabelTKN = 0xfa,
        kMacroTKN = 0xfc,
        kErrlnTKN = 0xfd,       // "is BlankTKN by itself"
        kBlankTKN = 0xfd,
        kCommentTKN = 0xfe,
        kComntStarTKN = 0xfe,
        kComntSemiTKN = 0xff,
    };

    typedef enum OperandResult {
        kResultUnknown = 0,
        kResultFailed,
        kResultGotoOutOprtr,
        kResultGotoOutOprnd,
    } OperandResult;

    void ProcessLine(const uint8_t* codePtr, int len);
    void ConvertOperand(uint8_t mnemonic, const uint8_t** pCodePtr,
        int* pLen);
    void PrintSymEntry(int ent);
    OperandResult PrintNum(uint8_t opr, const uint8_t** pCodePtr,
        int* pLen);
    OperandResult PrintComplexOperand(uint8_t opr,
        const uint8_t** pCodePtr, int* pLen);
    void PrintDec(int count, const uint8_t** pCodePtr, int* pLen);
    void PrintHex(int count, const uint8_t** pCodePtr, int* pLen);
    void PrintBin(int count, const uint8_t** pCodePtr, int* pLen);

    const uint8_t** fSymTab;
    uint16_t    fSymCount;

    uint8_t     fOpTab;
    uint8_t     fAdTab;
    uint8_t     fComTab;
    uint8_t     fCpuType;
};

#endif /*REFORMAT_ASM_H*/
