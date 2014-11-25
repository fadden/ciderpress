/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * 6502/65C02/65802/65816 disassembly.
 *
 * Lots of credit to the classic "Programming the 65816" book by David Eyes
 * and Ron Lichty.
 */
#ifndef REFORMAT_DISASM_H
#define REFORMAT_DISASM_H

#include "ReformatBase.h"

/*
 * Disassembly base class.  Common code and data for all formats.
 */
class ReformatDisasm65xxx : public ReformatText {
public:
    ReformatDisasm65xxx(void) {
        fOneByteBrkCop = false;
        ValidateOpMap();
        ValidateOpCodeDetails();
    }
    virtual ~ReformatDisasm65xxx(void) {}

    /*
     * OpCode enumeration, must fit in 0-255.
     */
    typedef enum OpCode {
        kOpCodeUnknown = 0,

        kOpADC,     kOpAND,     kOpASL,     kOpBCC,
        kOpBCS,     kOpBEQ,     kOpBIT,     kOpBMI,
        kOpBNE,     kOpBPL,     kOpBRA,     kOpBRK,
        kOpBRL,     kOpBVC,     kOpBVS,     kOpCLC,
        kOpCLD,     kOpCLI,     kOpCLV,     kOpCMP,
        kOpCOP,     kOpCPX,     kOpCPY,     kOpDEC,
        kOpDEX,     kOpDEY,     kOpEOR,     kOpINC,
        kOpINX,     kOpINY,     kOpJML,     kOpJMP,
        kOpJSL,     kOpJSR,     kOpLDA,     kOpLDX,
        kOpLDY,     kOpLSR,     kOpMVN,     kOpMVP,
        kOpNOP,     kOpORA,     kOpPEA,     kOpPEI,
        kOpPER,     kOpPHA,     kOpPHB,     kOpPHD,
        kOpPHK,     kOpPHP,     kOpPHX,     kOpPHY,
        kOpPLA,     kOpPLB,     kOpPLD,     kOpPLP,
        kOpPLX,     kOpPLY,     kOpREP,     kOpROL,
        kOpROR,     kOpRTI,     kOpRTL,     kOpRTS,
        kOpSBC,     kOpSEC,     kOpSED,     kOpSEI,
        kOpSEP,     kOpSTA,     kOpSTP,     kOpSTX,
        kOpSTY,     kOpSTZ,     kOpTAX,     kOpTAY,
        kOpTCD,     kOpTCS,     kOpTDC,     kOpTRB,
        kOpTSB,     kOpTSC,     kOpTSX,     kOpTXA,
        kOpTXS,     kOpTXY,     kOpTYA,     kOpTYX,
        kOpWAI,     kOpWDM,     kOpXBA,     kOpXCE,
        
        kOpCodeMAX
    } OpCode;

    /*
     * Address mode enumeration, must fit in 0-255.
     */
    typedef enum AddrMode {
        kAddrModeUnknown = 0,

        kAddrAbs,
        kAddrAbsIndexX,
        kAddrAbsIndexY,
        kAddrAbsIndexXInd,
        kAddrAbsInd,
        kAddrAbsIndLong,
        kAddrAbsLong,
        kAddrAbsIndexXLong,
        kAddrAcc,
        kAddrBlockMove,
        kAddrDP,
        kAddrDPIndexX,
        kAddrDPIndexY,
        kAddrDPIndexXInd,
        kAddrDPInd,
        kAddrDPIndLong,
        kAddrDPIndIndexY,
        kAddrDPIndIndexYLong,
        kAddrImm,
        kAddrImplied,
        kAddrPCRel,
        kAddrPCRelLong,
        kAddrStackAbs,
        kAddrStackDPInd,
        kAddrStackInt,
        kAddrStackPCRel,
        kAddrStackPull,
        kAddrStackPush,
        kAddrStackRTI,
        kAddrStackRTL,
        kAddrStackRTS,
        kAddrStackRel,
        kAddrStackRelIndexY,
        kAddrWDM,

        kAddrModeMAX
    } AddrMode;

    /*
     * Specify the CPU to emulate when disassembling.
     */
    typedef enum CPU {
        kCPU6502,
        kCPU65C02,
        kCPU65802,
        kCPU65816,

        kCPUCount
    } CPU;

protected:
    /*
     * Table mapping instruction byte values to an opcode and address mode.
     *
     * There are 256 entries, each of which is a combination of OpCode and
     * AddrMode.
     */
    typedef struct OpMap {
        int         opAndAddr[kCPUCount];
    } OpMap;

    /*
     * We have a table of these, with one entry in the table for each
     * entry in the OpCode enum.  We use a 1:1 mapping, so it's vital that
     * the entries line up.
     */
    typedef struct OpCodeDetails {
        OpCode      opCode;         // sanity check
        char        mnemonic[4];    // 3-letter mnemonic
    } OpCodeDetails;

    bool    fOneByteBrkCop;

    /*
     * Output one or more lines of code in a manner appropriate for a
     * monitor listing on an 8-bit machine.  Returns the #of bytes consumed.
     */
    int OutputMonitor8(const uint8_t* srcBuf, long srcLen,
        long backState, uint16_t addr);
    int OutputMonitor16(const uint8_t* srcBuf, long srcLen,
        long backState, uint32_t addr, bool shortRegs);

private:
    bool ValidateOpMap(void);
    static const OpMap kOpMap[];

    bool ValidateOpCodeDetails(void);
    static const OpCodeDetails kOpCodeDetails[];

    inline uint16_t RelOffset(uint16_t addr, uint8_t off) {
        int8_t shift = (int8_t) off;    // offset is signed
        return addr +2 + shift;
    }
    inline uint16_t RelLongOffset(uint16_t addr, uint16_t off) {
        int16_t shift = (int16_t) off;  // offset is signed
        return addr +3 + shift;
    }

    enum {
        kMaxByteConsumption = 4,    // max #of bytes for one output
    };

    int GetOpWidth(OpCode opCode, AddrMode addrMode, CPU cpu,
        bool emul, bool shortM, bool shortX);
    bool IsP8Call(const uint8_t* srcBuf, long srcLen);
    bool IsToolboxCall(const uint8_t* srcBuf, long srcLen,
        long backState);
    bool IsInlineGSOS(const uint8_t* srcBuf, long srcLen);
    bool IsStackGSOS(const uint8_t* srcBuf, long srcLen,
        long backState);

    void PrintMonitor8Line(OpCode opCode, AddrMode addrMode,
        uint16_t addr, const uint8_t* srcBuf, long srcLen,
        const char* comment);
    void PrintMonitor16Line(OpCode opCode, AddrMode addrMode,
        uint32_t addr, const uint8_t* srcBuf, long srcLen,
        const char* comment);

    /* 24-bit address helpers */
    inline uint8_t Bank(uint32_t addr) {
        return (uint8_t) ((addr >> 16) & 0xff);
    }
    inline uint16_t Offset(uint32_t addr) {
        return (uint16_t) (addr & 0xffff);
    }
};

/*
 * 8-bit code disassembly.
 *
 * Used for DOS 'B' files and ProDOS BIN/SYS.
 */
class ReformatDisasm8 : public ReformatDisasm65xxx {
public:
    ReformatDisasm8(void) {}
    virtual ~ReformatDisasm8(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;
};

/*
 * This holds an OMF segment header, and abstracts away version
 * differences.
 */
class OMFSegmentHeader {
public:
    OMFSegmentHeader(void) : fReady(false) {}
    virtual ~OMFSegmentHeader(void) {}

    bool Unpack(const uint8_t* srcBuf, long srcLen, int fileType);
    void Dump(void) const;

    typedef enum SegmentType {
        kTypeCode           = 0x00,
        kTypeData           = 0x01,
        kTypeJumpTable      = 0x02,
        kTypePathName       = 0x04,
        kTypeLibraryDict    = 0x08,
        kTypeInit           = 0x10,
        kTypeAbsoluteBank   = 0x11,
        kTypeDPStack        = 0x12,
    } SegmentType;
    typedef enum SegmentFlag {
        kFlagBankRelative,
        kFlagSkip,
        kFlagReload,
        kFlagAbsoluteBank,
        kFlagNoSpecialMem,
        kFlagPositionIndep,
        kFlagPrivate,
        kFlagDynamic,
    } SegmentFlag;

    uint8_t GetVersion(void) const { return fVersion; }
    uint32_t GetSegmentLen(void) const { return fByteCnt; }
    SegmentType GetSegmentType(void) const;
    bool GetSegmentFlag(SegmentFlag flag) const;
    uint16_t GetSegNum(void) const { return fSegNum; }
    uint8_t GetLabLen(void) const { return fLabLen; }
    uint8_t GetNumLen(void) const { return fNumLen; }
    uint16_t GetDispData(void) const { return fDispData; }
    const char* GetLoadName(void) const { return (const char*) fLoadName; }
    const char* GetSegName(void) const { return (const char*) fSegName; }

    // some of these must be public in VC++6.0
    enum {
        kMaxVersion         = 2,            // v0/1/2 == OMF 1.0 / 2.0 / 2.1
        kLoadNameLen        = 10,
        kV0HdrMinSize       = 0x25,
        kV1HdrMinSize       = 0x2c + kLoadNameLen,
        kV2HdrMinSize       = 0x30 + kLoadNameLen,
        kHdrMinSize         = kV0HdrMinSize,    // smallest of the three
        kExpectedNumLen     = 4,
        kExpectedBankSize   = 65536,

        kSegNameLen         = 255,
    };

private:
    bool            fReady;

    inline uint16_t Get16LE(const uint8_t* buf) {
        return *buf | *(buf+1) << 8;
    }
    inline uint32_t Get32LE(const uint8_t* buf) {
        return *buf | *(buf+1) << 8 | *(buf+2) << 16 | *(buf+3) << 24;
    }

    uint32_t    fBlockCnt;          // v0/v1
    uint32_t    fByteCnt;           // v2
    uint32_t    fResSpc;
    uint32_t    fLength;
    uint8_t     fType;              // v0/v1
    uint8_t     fLabLen;
    uint8_t     fNumLen;            // (always 4)
    uint8_t     fVersion;           // (0, 1, or 2)
    uint32_t    fBankSize;          // (always 65536)
    uint16_t    fKind;              // v2
    uint32_t    fOrg;
    uint32_t    fAlign;
    uint8_t     fNumSex;
    uint8_t     fLCBank;            // v1
    uint16_t    fSegNum;            // v1/v2
    uint32_t    fEntry;             // v1/v2
    uint16_t    fDispName;          // v1/v2
    uint16_t    fDispData;          // v1/v2
    uint32_t    fTempOrg;           // v2
    uint8_t     fLoadName[kLoadNameLen + 1];  // 10 chars, space-padded
    uint8_t     fSegName[kSegNameLen + 1];    // 1-255 chars
};

#if 0
/*
 * Constants and functions for manipulating an OMF segment.
 */
class OMFSegment {
public:
    OMFSegment(void) : fSegBuf(NULL), fSegLen(-1), fCurPtr(NULL),
        fNumLen(-1), fLabLen(-1)
        {}
    virtual ~OMFSegment(void) {}

    void Setup(const OMFSegmentHeader* pSegHdr, const uint8_t* srcBuf,
        long srcLen);
    const uint8_t* ProcessNextChunk(void);

    /*
     * Segment op codes.
     */
    typedef enum SegmentOp {
        kSegOpEND           = 0x00,     // all
        kSegOpCONSTStart    = 0x01,     // object
        kSegOpCONSTEnd      = 0xdf,     // object
        kSegOpALIGN         = 0xe0,     // object
        kSegOpORG           = 0xe1,     // org
        kSegOpRELOC         = 0xe2,     // load
        kSegOpINTERSEG      = 0xe3,     // load
        kSegOpUSING         = 0xe4,     // object
        kSegOpSTRONG        = 0xe5,     // object
        kSegOpGLOBAL        = 0xe6,     // object
        kSegOpGEQU          = 0xe7,     // object
        kSegOpMEM           = 0xe8,     // object
        // 0xe9, 0xea unused
        kSegOpEXPR          = 0xeb,     // object
        kSegOpZEXPR         = 0xec,     // object
        kSegOpBEXPR         = 0xed,     // object
        kSegOpRELEXPR       = 0xee,     // object
        kSegOpLOCAL         = 0xef,     // object
        kSegOpEQU           = 0xf0,     // object
        kSegOpDS            = 0xf1,     // all
        kSegOpLCONST        = 0xf2,     // all
        kSegOpLEXPR         = 0xf3,     // object
        kSegOpENTRY         = 0xf4,     // RTL
        kSegOpcRELOC        = 0xf5,     // load
        kSegOpcINTERSEG     = 0xf6,     // load
        kSegOpSUPER         = 0xf7,     // load

        kSegOpGeneral       = 0xfb,     // reserved
        kSegOpExperimental1 = 0xfc,     // reserved
        kSegOpExperimental2 = 0xfd,     // reserved
        kSegOpExperimental3 = 0xfe,     // reserved
        kSegOpExperimental4 = 0xff,     // reserved
    } SegmentOp;

    /*
     * Expression operands.
     */
    typedef enum ExprOp {
        kExprOpEnd              = 0x00,
        kExprOpAddition         = 0x01,
        kExprOpSubtraction      = 0x02,
        kExprOpMultiplication   = 0x03,
        kExprOpDivision         = 0x04,
        kExprOpIntegerRemainder = 0x05,
        kExprOpUnaryNegation    = 0x06,
        kExprOpBitShift         = 0x07,
        kExprOpAND              = 0x08,
        kExprOpOR               = 0x09,
        kExprOpEOR              = 0x0a,
        kExprOpNOT              = 0x0b,
        kExprOpLessThenEqualTo  = 0x0c,
        kExprOpGreaterThanEqualTo = 0x0d,
        kExprOpNotEqual         = 0x0e,
        kExprOpLessThan         = 0x0f,
        kExprOpGreaterThan      = 0x10,
        kExprOpEqualTo          = 0x11,
        kExprOpBitAND           = 0x12,
        kExprOpBitOR            = 0x13,
        kExprOpBitEOR           = 0x14,
        kExprOpBitNOT           = 0x15,

        kExprOpPushLocation     = 0x80,
        kExprOpPushConstant     = 0x81,
        kExprOpPushLabelWeak    = 0x82,
        kExprOpPushLabelValue   = 0x83,
        kExprOpPushLabelLength  = 0x84,
        kExprOpPushLabelType    = 0x85,
        kExprOpPushLabelCount   = 0x86,
        kExprOpPushRelOffset    = 0x87,
    } ExprOp;

private:
//  inline uint16_t Get16LE(const uint8_t* buf) {
//      return *buf | *(buf+1) << 8;
//  }
    inline uint32_t Get32LE(const uint8_t* buf) {
        return *buf | *(buf+1) << 8 | *(buf+2) << 16 | *(buf+3) << 24;
    }

    /*
     * Given a pointer to the start of a label, return the label's len.
     * The length returned includes the length byte (if present).
     */
    int LabelLength(const uint8_t* ptr) {
        if (fLabLen != 0)
            return fLabLen;
        else
            return (*ptr) +1;
    }

    /* determine the length of an expression */
    int ExpressionLength(const uint8_t* ptr);

    const uint8_t*          fSegBuf;
    long                    fSegLen;

    const uint8_t*          fCurPtr;
    int                     fNumLen;
    int                     fLabLen;
};
#endif


/*
 * 16-bit code disassembly.
 *
 * Used for GS/OS S16, EXE, PIF, and others.
 *
 * OMF information comes from Appendix F of the Apple IIgs GS/OS Reference
 * manual and Appendix B of the Orca/M assembler manual.
 */
class ReformatDisasm16 : public ReformatDisasm65xxx {
public:
    ReformatDisasm16(void) {}
    virtual ~ReformatDisasm16(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
    void OutputSection(const uint8_t* srcBuf, long srcLen,
        uint32_t addr, bool shortRegs);
    bool OutputOMF(const uint8_t* srcBuf, long srcLen,
        long fileType, bool shortRegs);
    void PrintHeader(const OMFSegmentHeader* pSegHdr,
        int segmentNumber, bool longFmt);
    void PrintSegment(const OMFSegmentHeader* pSegHdr,
        const uint8_t* srcBuf, long srcLen, bool shortRegs);
};

#endif /*REFORMAT_DISASM_H*/
