/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Disassemble Apple II binaries.
 */
#include "StdAfx.h"
#include "Disasm.h"

/*
 * ===========================================================================
 *      Common stuff for 65xxx-series CPU
 * ===========================================================================
 */

/*
 * The color scheme from Don Lancaster's "Enhancing your Apple II, Volume 1,
 * Second Edition":
 *  (1) Paint all subroutine returns (RTS) green (entire line to the right
 *      of the 4-character address field).
 *  (2) Paint all subroutine calls (JSR) orange (with an extended swipe for
 *      out-of-range calls, e.g. JSR $FDED).  Paint the target address of
 *      the JSR orange as well.  Write the name of the target in brown felt
 *      tip after the orange arrow, e.g. "COUT (output char)".
 *  (3) Paint all absolute jumps pink.  Highlight across the entire line
 *      (everything but the address).  Paint the address field of the target
 *      location pink.
 *  (4) Show branches in blue, with lines down the left-hand side.
 *  (5) Paint constants green ("lda #$74") and variables pink ("sta $38"),
 *      highlighting only the operation (i.e. not the hex values in the
 *      middle of the listing).
 *  (6) Paint the housekeeping yellow (inx, dex, dey, txa, cld, sec, tsx).
 *      Draw yellow lines between pha/pla pairs.
 */

/*
 * Return the #of bytes required by the specified addressing mode, given a
 * particular CPU and values for the 'e', 'm', and 'x' flags.
 *
 * Width is always > 0.  This guarantees that we don't loop forever even if
 * we hit stuff we don't recognize.
 */
int ReformatDisasm65xxx::GetOpWidth(OpCode opCode, AddrMode addrMode, CPU cpu,
    bool emul, bool shortM, bool shortX)
{
    int width = 0;

    switch (addrMode) {
    case kAddrAcc:
    case kAddrImplied:
    case kAddrStackPull:
    case kAddrStackPush:
    case kAddrStackRTI:
    case kAddrStackRTL:
    case kAddrStackRTS:
        width = 1;
        break;
    case kAddrDP:
    case kAddrDPIndexX:
    case kAddrDPIndexY:
    case kAddrDPIndexXInd:
    case kAddrDPInd:
    case kAddrDPIndLong:
    case kAddrDPIndIndexY:
    case kAddrDPIndIndexYLong:
    case kAddrPCRel:
    case kAddrStackRel:
    case kAddrStackRelIndexY:
    case kAddrStackDPInd:
        width = 2;
        break;
    case kAddrAbs:
    case kAddrAbsIndexX:
    case kAddrAbsIndexY:
    case kAddrAbsIndexXInd:
    case kAddrAbsInd:
    case kAddrAbsIndLong:
    case kAddrBlockMove:
    case kAddrPCRelLong:
    case kAddrStackAbs:
    case kAddrStackPCRel:
        width = 3;
        break;
    case kAddrAbsLong:
    case kAddrAbsIndexXLong:
        width = 4;
        break;
    case kAddrStackInt:     // BRK/COP
        if (emul || fOneByteBrkCop)
            width = 1;
        else
            width = 2;
        break;
    case kAddrImm:
        width = 2;
        if (!emul) {
            if (opCode == kOpCPX ||
                opCode == kOpCPY ||
                opCode == kOpLDX ||
                opCode == kOpLDY)
            {
                if (!shortX)
                    width = 3;
            } else
            if (opCode == kOpADC ||
                opCode == kOpAND ||
                opCode == kOpBIT ||
                opCode == kOpCMP ||
                opCode == kOpEOR ||
                opCode == kOpLDA ||
                opCode == kOpORA ||
                opCode == kOpSBC)
            {
                if (!shortM)
                    width = 3;
            } else
            if (opCode == kOpREP ||
                opCode == kOpSEP)
            {
                /* keep width = 2 */
            } else {
                assert(false);
            }
        }
        break;
    case kAddrWDM:              // not really defined
        width = 2;
        break;

    case kAddrModeUnknown:      // unknown opcode
        width = 1;
        break;
    default:
        assert(false);
        width = 1;
        break;
    }

    assert(width > 0);
    return width;
}

/*
 * Returns "true" if it looks like we're pointing at a ProDOS 8 MLI call.
 *
 * The caller is expected to check to see if we're in bank 0.
 */
bool ReformatDisasm65xxx::IsP8Call(const uint8_t* srcBuf, long srcLen)
{
    if (srcLen >= 6 &&
        srcBuf[0] == 0x20 && srcBuf[1] == 0x00 && srcBuf[2] == 0xbf)
    {
        return true;
    }
    return false;
}

/*
 * Returns "true" if it looks like we're pointing at a IIgs toolbox call.
 */
bool ReformatDisasm65xxx::IsToolboxCall(const uint8_t* srcBuf, long srcLen,
    long backState)
{
    if (srcLen >= 4 && backState >= 3 &&
        srcBuf[0] == 0x22 && srcBuf[1] == 0x00 && srcBuf[2] == 0x00 &&
        srcBuf[3] == 0xe1 && srcBuf[-3] == 0xa2)    // LDX #xxxx
    {
        return true;
    }
    return false;
}

/*
 * Returns "true" if it looks like we're pointing at an inline GS/OS call.
 */
bool ReformatDisasm65xxx::IsInlineGSOS(const uint8_t* srcBuf, long srcLen)
{
    if (srcLen >= 10 &&
        srcBuf[0] == 0x22 && srcBuf[1] == 0xa8 && srcBuf[2] == 0x00 &&
        srcBuf[3] == 0xe1)
    {
        return true;
    }
    return false;
}

/*
 * Returns "true" if it looks like we're pointing at a stack GS/OS call.
 */
bool ReformatDisasm65xxx::IsStackGSOS(const uint8_t* srcBuf, long srcLen,
    long backState)
{
    if (srcLen >= 4 && backState >= 3 &&
        srcBuf[0] == 0x22 && srcBuf[1] == 0xb0 && srcBuf[2] == 0x00 &&
        srcBuf[3] == 0xe1 && srcBuf[-3] == 0xf4)    // PEA xxxx
    {
        return true;
    }
    return false;
}


/*
 * Output one or more lines of text similar to what the monitor would output
 * on an 8-bit Apple II.
 *
 * Returns the number of bytes consumed.
 */
int ReformatDisasm65xxx::OutputMonitor8(const uint8_t* srcBuf, long srcLen,
    long backState, uint16_t addr)
{
    const CPU kCPU = kCPU65C02;     // 6502 or 65C02
    OpCode opCode;
    AddrMode addrMode;
    int bytesUsed;
    int opAndAddr;

    opAndAddr = kOpMap[*srcBuf].opAndAddr[kCPU];
    opCode = (OpCode) (opAndAddr & 0xff);
    assert(opCode >= kOpCodeUnknown && opCode < kOpCodeMAX);
    addrMode = (AddrMode) (opAndAddr >> 8);
    assert(addrMode >= kAddrModeUnknown && addrMode < kAddrModeMAX);

    bytesUsed = GetOpWidth(opCode, addrMode, kCPU, true, true, true);

    if (IsP8Call(srcBuf, srcLen)) {
        /* print and skip P8 inline call stuff */
        const char* callName;

        callName = NiftyList::LookupP8MLI(srcBuf[3]);
        if (callName == NULL)
            callName = "(Unknown P8 MLI)";
        PrintMonitor8Line(opCode, addrMode, addr, srcBuf, bytesUsed, callName);
        BufPrintf("%04X-   %02X                $%02X",
            addr+3, srcBuf[3], srcBuf[3]);
        RTFNewPara();
        BufPrintf("%04X-   %02X %02X             $%02X%02X",
            addr+4, srcBuf[4], srcBuf[5], srcBuf[5], srcBuf[4]);
        RTFNewPara();
        bytesUsed += 3;
    } else
    if (srcLen < bytesUsed) {
        assert(bytesUsed <= kMaxByteConsumption);
        uint8_t tmpBuf[kMaxByteConsumption];
        memset(tmpBuf, 0, kMaxByteConsumption);
        memcpy(tmpBuf, srcBuf, srcLen);

        PrintMonitor8Line(opCode, addrMode, addr, tmpBuf, bytesUsed, NULL);
    } else {
        PrintMonitor8Line(opCode, addrMode, addr, srcBuf, bytesUsed, NULL);
    }


    return bytesUsed;
}

/*
 * Output one line of 8-bit monitor stuff.
 */
void ReformatDisasm65xxx::PrintMonitor8Line(OpCode opCode, AddrMode addrMode,
    uint16_t addr, const uint8_t* srcBuf, long srcLen, const char* comment)
{
    char lineBuf[64];   // actual length is about 30 -- does not hold comment
    char* cp;
    const char* mnemonic = kOpCodeDetails[opCode].mnemonic;
    uint8_t byte0, byte1, byte2;

    cp = lineBuf;

    switch (srcLen) {
    case 1:
        byte0 = srcBuf[0];
        byte1 = byte2 = 0xcc;       // make bugs more obvious
        cp += sprintf(cp, "%04X-   %02X          %s", addr, byte0, mnemonic);
        break;
    case 2:
        byte0 = srcBuf[0];
        byte1 = srcBuf[1];
        byte2 = 0xcc;
        cp += sprintf(cp, "%04X-   %02X %02X       %s", addr, byte0, byte1,
            mnemonic);
        break;
    case 3:
        byte0 = srcBuf[0];
        byte1 = srcBuf[1];
        byte2 = srcBuf[2];
        cp += sprintf(cp, "%04X-   %02X %02X %02X    %s", addr, byte0, byte1,
            byte2, mnemonic);
        break;
    default:
        assert(false);
        return;
    }

    switch (addrMode) {
    case kAddrImplied:
    case kAddrStackPull:
    case kAddrStackPush:
    case kAddrStackRTI:
    case kAddrStackRTS:
        break;
    case kAddrAcc:
        //cp += sprintf(cp, "   A");
        break;

    case kAddrDP:
        cp += sprintf(cp, "   $%02X", byte1);
        break;
    case kAddrDPIndexX:
        cp += sprintf(cp, "   $%02X,X", byte1);
        break;
    case kAddrDPIndexY:
        cp += sprintf(cp, "   $%02X,Y", byte1);
        break;
    case kAddrDPIndexXInd:
        cp += sprintf(cp, "   ($%02X,X)", byte1);
        break;
    case kAddrDPInd:
        cp += sprintf(cp, "   ($%02X)", byte1);
        break;
    case kAddrDPIndIndexY:
        cp += sprintf(cp, "   ($%02X),Y", byte1);
        break;
    case kAddrImm:
        cp += sprintf(cp, "   #$%02X", byte1);
        break;
    case kAddrPCRel:
        cp += sprintf(cp, "   $%04X", RelOffset(addr, byte1));
        break;

    case kAddrAbs:
        cp += sprintf(cp, "   $%02X%02X", byte2, byte1);
        if (comment == NULL)
            comment = NiftyList::Lookup00Addr(byte1 | byte2 << 8);
        break;
    case kAddrAbsIndexX:
        cp += sprintf(cp, "   $%02X%02X,X", byte2, byte1);
        break;
    case kAddrAbsIndexY:
        cp += sprintf(cp, "   $%02X%02X,Y", byte2, byte1);
        break;
    case kAddrAbsIndexXInd:
        cp += sprintf(cp, "   ($%02X%02X,X)", byte2, byte1);
        break;
    case kAddrAbsInd:
        cp += sprintf(cp, "   ($%02X%02X)", byte2, byte1);
        break;
    case kAddrStackInt:         // BRK/COP
        if (srcLen != 1)
            cp += sprintf(cp, "   $%02X", byte1);
        break;

    case kAddrAbsIndLong:       // JML
    case kAddrAbsLong:
    case kAddrAbsIndexXLong:
    case kAddrBlockMove:
    case kAddrDPIndLong:
    case kAddrDPIndIndexYLong:
    case kAddrPCRelLong:        // BRL
    case kAddrStackAbs:         // PEA
    case kAddrStackDPInd:       // PEI
    case kAddrStackPCRel:       // PER
    case kAddrStackRTL:
    case kAddrStackRel:
    case kAddrStackRelIndexY:
    case kAddrWDM:
        // not for 8-bit mode
        assert(false);
        break;
    case kAddrModeUnknown:
        assert(srcLen == 1);
        break;
    default:
        assert(false);
        break;
    }

    assert(strlen(cp)+1 < sizeof(lineBuf));
    if (comment == NULL)
        BufPrintf("%s", lineBuf);
    else
        BufPrintf("%s    %s", lineBuf, comment);
    RTFNewPara();
}

/*
 * Output one or more lines of text similar to what the monitor would output
 * on an 8-bit Apple II.
 *
 * Returns the number of bytes consumed.
 */
int ReformatDisasm65xxx::OutputMonitor16(const uint8_t* srcBuf, long srcLen,
    long backState, uint32_t addr, bool shortRegs)
{
    const CPU kCPU = kCPU65816;
    OpCode opCode;
    AddrMode addrMode;
    int bytesUsed;
    int opAndAddr;
    const char* callName;

    opAndAddr = kOpMap[*srcBuf].opAndAddr[kCPU];
    opCode = (OpCode) (opAndAddr & 0xff);
    assert(opCode >= kOpCodeUnknown && opCode < kOpCodeMAX);
    addrMode = (AddrMode) (opAndAddr >> 8);
    assert(addrMode >= kAddrModeUnknown && addrMode < kAddrModeMAX);

    bytesUsed = GetOpWidth(opCode, addrMode, kCPU, false, shortRegs, shortRegs);

    if (Bank(addr) == 0 && IsP8Call(srcBuf, srcLen)) {
        /* print and skip P8 inline call stuff */
        callName = NiftyList::LookupP8MLI(srcBuf[3]);
        if (callName == NULL)
            callName = "(Unknown P8 MLI)";
        PrintMonitor16Line(opCode, addrMode, addr, srcBuf, bytesUsed, callName);
        BufPrintf("00/%04X: %02X                %02X",
            addr+3, srcBuf[3], srcBuf[3]);
        RTFNewPara();
        BufPrintf("00/%04X: %02X %02X             %02X%02X",
            addr+4, srcBuf[4], srcBuf[5], srcBuf[5], srcBuf[4]);
        RTFNewPara();
        bytesUsed += 3;
    } else
    if (IsToolboxCall(srcBuf, srcLen, backState)) {
        callName = NiftyList::LookupToolbox(srcBuf[-2] | srcBuf[-1] << 8);
        PrintMonitor16Line(opCode, addrMode, addr, srcBuf, bytesUsed, callName);
    } else
    if (IsInlineGSOS(srcBuf, srcLen)) {
        callName = NiftyList::LookupGSOS(srcBuf[4] | srcBuf[5] << 8);
        PrintMonitor16Line(opCode, addrMode, addr, srcBuf, bytesUsed, callName);
        BufPrintf("%02X/%04X: %02X %02X             %02X%02X",
            Bank(addr), Offset(addr+4), srcBuf[4], srcBuf[5],
            srcBuf[5], srcBuf[4]);
        RTFNewPara();
        BufPrintf("%02X/%04X: %02X %02X %02X %02X       %02X%02X%02X%02X",
            Bank(addr), Offset(addr+6),
            srcBuf[6], srcBuf[7], srcBuf[8], srcBuf[9],
            srcBuf[9], srcBuf[8], srcBuf[7], srcBuf[6]);
        RTFNewPara();
        bytesUsed += 6;
    } else
    if (IsStackGSOS(srcBuf, srcLen, backState)) {
        callName = NiftyList::LookupGSOS(srcBuf[-2] | srcBuf[-1] << 8);
        PrintMonitor16Line(opCode, addrMode, addr, srcBuf, bytesUsed, callName);
    } else
    if (srcLen < bytesUsed) {
        assert(bytesUsed <= kMaxByteConsumption);
        uint8_t tmpBuf[kMaxByteConsumption];
        memset(tmpBuf, 0, kMaxByteConsumption);
        memcpy(tmpBuf, srcBuf, srcLen);

        PrintMonitor16Line(opCode, addrMode, addr, tmpBuf, bytesUsed, NULL);
    } else {
        PrintMonitor16Line(opCode, addrMode, addr, srcBuf, bytesUsed, NULL);
    }

    return bytesUsed;
}

/*
 * Output one line of 16-bit monitor stuff.
 */
void ReformatDisasm65xxx::PrintMonitor16Line(OpCode opCode, AddrMode addrMode,
    uint32_t addr, const uint8_t* srcBuf, long srcLen, const char* comment)
{
    char lineBuf[64];   // actual length is about 30 -- does not hold comment
    char* cp;
    const char* mnemonic = kOpCodeDetails[opCode].mnemonic;
    uint8_t byte0, byte1, byte2, byte3;
    int16_t offset;

    cp = lineBuf;

    cp += sprintf(cp, "%02X/%04X: ", addr >> 16, addr & 0xffff);

    switch (srcLen) {
    case 1:
        byte0 = srcBuf[0];
        byte1 = byte2 = byte3 = 0xcc;       // make bugs more obvious
        cp += sprintf(cp, "%02X            %s", byte0, mnemonic);
        break;
    case 2:
        byte0 = srcBuf[0];
        byte1 = srcBuf[1];
        byte2 = byte3 = 0xcc;
        cp += sprintf(cp, "%02X %02X         %s", byte0, byte1, mnemonic);
        break;
    case 3:
        byte0 = srcBuf[0];
        byte1 = srcBuf[1];
        byte2 = srcBuf[2];
        byte3 = 0xcc;
        cp += sprintf(cp, "%02X %02X %02X      %s", byte0, byte1, byte2,
            mnemonic);
        break;
    case 4:
        byte0 = srcBuf[0];
        byte1 = srcBuf[1];
        byte2 = srcBuf[2];
        byte3 = srcBuf[3];
        cp += sprintf(cp, "%02X %02X %02X %02X   %s", byte0, byte1, byte2,
            byte3, mnemonic);
        break;
    default:
        assert(false);
        return;
    }

    switch (addrMode) {
    case kAddrImplied:
    case kAddrStackPull:
    case kAddrStackPush:
    case kAddrStackRTI:
    case kAddrStackRTS:
    case kAddrStackRTL:
        break;
    case kAddrAcc:
        //cp += sprintf(cp, "   A");
        break;

    case kAddrDP:
        cp += sprintf(cp, " %02X", byte1);
        break;
    case kAddrDPIndexX:
        cp += sprintf(cp, " %02X,X", byte1);
        break;
    case kAddrDPIndexY:
        cp += sprintf(cp, " %02X,Y", byte1);
        break;
    case kAddrDPIndexXInd:
        cp += sprintf(cp, " (%02X,X)", byte1);
        break;
    case kAddrDPInd:
        cp += sprintf(cp, " (%02X)", byte1);
        break;
    case kAddrDPIndIndexY:
        cp += sprintf(cp, " (%02X),Y", byte1);
        break;
    case kAddrImm:
        if (srcLen == 2)
            cp += sprintf(cp, " #%02X", byte1);
        else
            cp += sprintf(cp, " #%02X%02X", byte2, byte1);
        break;
    case kAddrPCRel:
        offset = (char) byte1;
        if (offset < 0)
            cp += sprintf(cp, " %04X {-%02X}",
                RelOffset((uint16_t) addr, byte1), -offset);
        else
            cp += sprintf(cp, " %04X {+%02X}",
                RelOffset((uint16_t) addr, byte1), offset);
        break;

    case kAddrAbs:
        cp += sprintf(cp, " %02X%02X", byte2, byte1);
        if (comment == NULL && Bank(addr) == 0)
            comment = NiftyList::Lookup00Addr(byte1 | byte2 << 8);
        break;
    case kAddrAbsIndexX:
        cp += sprintf(cp, " %02X%02X,X", byte2, byte1);
        break;
    case kAddrAbsIndexY:
        cp += sprintf(cp, " %02X%02X,Y", byte2, byte1);
        break;
    case kAddrAbsIndexXInd:
        cp += sprintf(cp, " (%02X%02X,X)", byte2, byte1);
        break;
    case kAddrAbsInd:
        cp += sprintf(cp, " (%02X%02X)", byte2, byte1);
        break;
    case kAddrWDM:
    case kAddrStackInt:         // BRK/COP
        if (srcLen != 1)
            cp += sprintf(cp, " %02X", byte1);
        break;

    case kAddrAbsIndLong:       // JML
        cp += sprintf(cp, " (%02X%02X)", byte2, byte1);
        break;
    case kAddrAbsLong:
        cp += sprintf(cp, " %02X%02X%02X", byte3, byte2, byte1);
        if (comment == NULL) {
            if (byte3 == 0x00)
                comment = NiftyList::Lookup00Addr(byte1 | byte2 << 8);
            else if (byte3 == 0x01)
                comment = NiftyList::Lookup01Vector(byte1 | byte2 << 8);
            else if (byte3 == 0xe0)
                comment = NiftyList::LookupE0Vector(byte1 | byte2 << 8);
            else if (byte3 == 0xe1)
                comment = NiftyList::LookupE1Vector(byte1 | byte2 << 8);
        }
        break;
    case kAddrAbsIndexXLong:
        cp += sprintf(cp, " %02X%02X%02X,X", byte3, byte2, byte1);
        break;
    case kAddrBlockMove:
        cp += sprintf(cp, " %02X%02X", byte2, byte1);
        break;
    case kAddrDPIndLong:
        cp += sprintf(cp, " [%02X]", byte1);
        break;
    case kAddrDPIndIndexYLong:
        cp += sprintf(cp, " [%02X],Y", byte1);
        break;
    case kAddrStackPCRel:       // PER
    case kAddrPCRelLong:        // BRL
        offset = (short) (byte1 | byte2 << 8);
        if (offset < 0)
            cp += sprintf(cp, " %04X {-%02X}",
                RelLongOffset((uint16_t) addr, offset), -offset);
        else
            cp += sprintf(cp, " %04X {+%02X}",
                RelLongOffset((uint16_t) addr, offset), offset);
        break;
    case kAddrStackAbs:         // PEA
        cp += sprintf(cp, " %02X%02X", byte2, byte1);
        break;
    case kAddrStackDPInd:       // PEI
        cp += sprintf(cp, " %02X", byte1);
        break;
    case kAddrStackRel:
        cp += sprintf(cp, " %02X,S", byte1);
        break;
    case kAddrStackRelIndexY:
        cp += sprintf(cp, " (%02X,S),Y", byte1);
        break;
    case kAddrModeUnknown:
        assert(srcLen == 1);
        break;
    default:
        assert(false);
        break;
    }

    assert(strlen(cp)+1 < sizeof(lineBuf));
    if (comment == NULL)
        BufPrintf("%s", lineBuf);
    else {
        if (srcLen < 4)
            BufPrintf("%s    %s", lineBuf, comment);
        else
            BufPrintf("%s  %s", lineBuf, comment);
    }
    RTFNewPara();
}


/*
 * ===========================================================================
 *      Disassemble 8-bit code
 * ===========================================================================
 */

/*
 * For ProDOS 8 and DOS 3.3 stuff, we can't say for sure whether a "BIN"
 * file is code or, say, a hi-res graphic.  We use "maybe" level to put
 * disassembly below other formats.
 */
void ReformatDisasm8::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;
    int fileType = pHolder->GetFileType();

    if (fileType >= 0xf1 && fileType <= 0xf8) {
        applies = ReformatHolder::kApplicProbablyNot;
    } else if (fileType == 0x00 &&
        pHolder->GetSourceFormat() != ReformatHolder::kSourceFormatCPM)
    {
        applies = ReformatHolder::kApplicProbablyNot;
    } else if (fileType == kTypeBIN) {
        applies = ReformatHolder::kApplicMaybe;
    } else if (fileType == kTypeSYS || fileType == kTypeCMD ||
        fileType == kType8OB || fileType == kTypeP8C)
    {
        applies = ReformatHolder::kApplicYes;
    }

    pHolder->SetApplic(ReformatHolder::kReformatMonitor8, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);

    // not doing this yet
    pHolder->SetApplic(ReformatHolder::kReformatDisasmMerlin8,
        ReformatHolder::kApplicNot,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Create a monitor listing or disassembly of a file.
 */
int ReformatDisasm8::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    long fileType = pHolder->GetFileType();
    long backState = 0;
    uint16_t addr;
    fUseRTF = false;

    if (!srcLen)
        return -1;

    RTFBegin();

    /* pick a nice 16-bit start address */
    if (fileType == kTypeSYS || fileType == kTypeP8C || fileType == kType8OB)
        addr = 0x2000;
    else if (fileType == kTypeBIN || fileType == kTypeCMD)
        addr = (uint16_t) pHolder->GetAuxType();
    else
        addr = 0x0000;

    while (srcLen > 0) {
        int consumed;

        consumed = OutputMonitor8(srcBuf, srcLen, backState, addr);

        srcBuf += consumed;
        srcLen -= consumed;
        backState += consumed;
        addr += consumed;
    }

    RTFEnd();
    SetResultBuffer(pOutput);
    return 0;
}


/*
 * ===========================================================================
 *      Disassemble 16-bit code
 * ===========================================================================
 */

/*
 * For 16-bit stuff, the file types are always explicit.  However, there are
 * instances of 65802/65816 code being written for 8-bit platforms, so we
 * want to keep 16-bit disassembly available but prioritized below "raw" for
 * BIN files.
 */
void ReformatDisasm16::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;
    long fileType = pHolder->GetFileType();

    if (fileType == kTypeBIN || fileType == kTypeSYS || fileType == kTypeCMD ||
        fileType == kType8OB || fileType == kTypeP8C)
    {
        applies = ReformatHolder::kApplicProbablyNot;
    } else if (fileType == 0x00 &&
        pHolder->GetSourceFormat() != ReformatHolder::kSourceFormatCPM)
    {
        applies = ReformatHolder::kApplicProbablyNot;
    } else {
        /*
         * Interesting file types (GS/OS ref table F-1):
         *  $B1 OBJ Object
         *  $B2 LIB Library
         *  $B3 S16 GS/OS or ProDOS 16 app
         *  $B4 RTL Run-time library
         *  $B5 EXE Shell application
         *  $B6 PIF Permanent initialization
         *  $B7 TIF Temporary initialization
         *  $B8 NDA New desk accessory
         *  $B9 CDA Classic desk accessory
         *  $BA TOL Tool set file
         *  $BB DVR Apple IIgs device driver
         *  $BC LDF Generic load file (application-specific)
         *  $BD FST GS/OS file system translator
         *
         * We also handle non-OMF files:
         *  $F9 OS  GS/OS System file
         */
        if (fileType >= kTypeOBJ && fileType <= kTypeFST)
            applies = ReformatHolder::kApplicYes;
        else if (fileType == kTypeOS)
            applies = ReformatHolder::kApplicYes;
    }

    pHolder->SetApplic(ReformatHolder::kReformatMonitor16Long, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    pHolder->SetApplic(ReformatHolder::kReformatMonitor16Short, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    pHolder->SetApplicPreferred(ReformatHolder::kReformatMonitor16Long);

    // not doing this yet
    pHolder->SetApplic(ReformatHolder::kReformatDisasmOrcam16,
        ReformatHolder::kApplicNot,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Disassemble or show monitor listing for 16-bit code.
 */
int ReformatDisasm16::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    long fileType = pHolder->GetFileType();
    uint32_t addr = 0;
    bool shortRegs;

    fUseRTF = false;

    if (!srcLen)
        return -1;

    RTFBegin();

    if (id == ReformatHolder::kReformatMonitor16Long)
        shortRegs = false;
    else
        shortRegs = true;
    fOneByteBrkCop = pHolder->GetOption(ReformatHolder::kOptOneByteBrkCop) != 0;

    if (fileType >= kTypeOBJ && fileType <= kTypeFST) {
        if (!OutputOMF(srcBuf, srcLen, fileType, shortRegs)) {
            /* must not be OMF; do a generic list */
            fExpBuf.Reset();
            BufPrintf("[ Valid OMF expected but not found ]\r\n");
            RTFNewPara();
            OutputSection(srcBuf, srcLen, addr, shortRegs);
        }
    } else {
        /* pick a start address in bank 0 */
        if (fileType == kTypeSYS)
            addr = 0x2000;
        else if (fileType == kTypeBIN || fileType == kTypeCMD)
            addr = (uint16_t) pHolder->GetAuxType();
        OutputSection(srcBuf, srcLen, addr, shortRegs);
    }

    RTFEnd();
    SetResultBuffer(pOutput);
    return 0;
}

/*
 * Output one section of a file.
 */
void ReformatDisasm16::OutputSection(const uint8_t* srcBuf, long srcLen,
    uint32_t addr, bool shortRegs)
{
    long backState = 0;

    while (srcLen > 0) {
        int consumed;

        consumed = OutputMonitor16(srcBuf, srcLen, backState, addr, shortRegs);

        srcBuf += consumed;
        srcLen -= consumed;
        backState += consumed;
        addr += consumed;
    }
}

/*
 * Break an OMF file into sections, and output each individually.
 */
bool ReformatDisasm16::OutputOMF(const uint8_t* srcBuf, long srcLen,
    long fileType, bool shortRegs)
{
    const uint8_t* origBuf = srcBuf;
    long origLen = srcLen;
    OMFSegmentHeader segHdr;
    int segmentNumber = 1;

    BufPrintf(";\r\n");
    BufPrintf("; OMF segment summary:\r\n");
    BufPrintf(";\r\n");

    /* pass #1: print a preview */
    while (srcLen > 0) {
        if (!segHdr.Unpack(srcBuf, srcLen, fileType)) {
            if (segmentNumber == 1)
                return false;
            else {
                BufPrintf("; (bad header found, ignoring last %ld bytes)\r\n",
                    srcLen);
                break;  // out of while; display what we have
            }
        }

        PrintHeader(&segHdr, segmentNumber, false);

        srcBuf += segHdr.GetSegmentLen();
        srcLen -= segHdr.GetSegmentLen();
        segmentNumber++;
    }
    BufPrintf(";\r\n");
    RTFNewPara();

    segmentNumber = 1;
    srcBuf = origBuf;
    srcLen = origLen;
    while (srcLen > 0) {
        if (!segHdr.Unpack(srcBuf, srcLen, fileType)) {
            BufPrintf("!!!\r\n");
            BufPrintf("!!! Found bad OMF header at offset 0x%04x (remaining len=%ld)\r\n",
                srcBuf - origBuf, srcLen);
            BufPrintf("!!!\r\n");
            RTFNewPara();
            if (segmentNumber == 1)
                return false;
            else
                return true;
        }

        PrintHeader(&segHdr, segmentNumber, true);

        PrintSegment(&segHdr, srcBuf, srcLen, shortRegs);
        RTFNewPara();

        srcBuf += segHdr.GetSegmentLen();
        srcLen -= segHdr.GetSegmentLen();
        segmentNumber++;
    }

    return true;
}

/*
 * Print the interesting bits of the header.
 */
void ReformatDisasm16::PrintHeader(const OMFSegmentHeader* pSegHdr,
    int segmentNumber, bool longFmt)
{
    //pSegHdr->Dump();

    const char* versStr;
    switch (pSegHdr->GetVersion()) {
    case 0:     versStr = "1.0";        break;
    case 1:     versStr = "2.0";        break;
    case 2:     versStr = "2.1";        break;
    default:    versStr = "(unknown)";  break;
    }

    const char* typeStr;
    switch (pSegHdr->GetSegmentType()) {
    case OMFSegmentHeader::kTypeCode:           typeStr = "CODE";       break;
    case OMFSegmentHeader::kTypeData:           typeStr = "DATA";       break;
    case OMFSegmentHeader::kTypeJumpTable:      typeStr = "JumpTab";    break;
    case OMFSegmentHeader::kTypePathName:       typeStr = "PathName";   break;
    case OMFSegmentHeader::kTypeLibraryDict:    typeStr = "LibDict";    break;
    case OMFSegmentHeader::kTypeInit:           typeStr = "Init";       break;
    case OMFSegmentHeader::kTypeAbsoluteBank:   typeStr = "AbsBank";    break;
    case OMFSegmentHeader::kTypeDPStack:        typeStr = "DP/Stack";   break;
    default:                                    typeStr = "(unknown)";  break;
    }

    if (longFmt) {
        BufPrintf(";\r\n");
        BufPrintf("; Segment #%d (%d): loadName='%s' segName='%s': \r\n",
            segmentNumber, pSegHdr->GetSegNum(),
            pSegHdr->GetLoadName(), pSegHdr->GetSegName());
        BufPrintf(";  type=%s  length=%ld  OMF v%s\r\n",
            typeStr, pSegHdr->GetSegmentLen(), versStr);
        BufPrintf(";  flags:%s%s%s%s%s%s%s%s\r\n",
            pSegHdr->GetSegmentFlag(OMFSegmentHeader::kFlagBankRelative) ? " bankRel" : "",
            pSegHdr->GetSegmentFlag(OMFSegmentHeader::kFlagSkip) ? " skip" : "",
            pSegHdr->GetSegmentFlag(OMFSegmentHeader::kFlagReload) ? " reload" : "",
            pSegHdr->GetSegmentFlag(OMFSegmentHeader::kFlagAbsoluteBank) ? " absBank" : "",
            pSegHdr->GetSegmentFlag(OMFSegmentHeader::kFlagNoSpecialMem) ? " noSpecial" : "",
            pSegHdr->GetSegmentFlag(OMFSegmentHeader::kFlagPositionIndep) ? " posnIndep" : "",
            pSegHdr->GetSegmentFlag(OMFSegmentHeader::kFlagPrivate) ? " private" : "",
            pSegHdr->GetSegmentFlag(OMFSegmentHeader::kFlagDynamic) ? " dynamic" : "");
        BufPrintf(";");
        RTFNewPara();
    } else {
        BufPrintf(";  #%02d: %-8s len=0x%06x  loadName='%s' segName='%s'\r\n",
            segmentNumber, typeStr, pSegHdr->GetSegmentLen(),
            pSegHdr->GetLoadName(), pSegHdr->GetSegName());
    }
}


/*
 * Print the contents of the segment.
 */
void ReformatDisasm16::PrintSegment(const OMFSegmentHeader* pSegHdr,
    const uint8_t* srcBuf, long srcLen, bool shortRegs)
{
    uint32_t subLen;
    int offset = 0;

    assert(pSegHdr != NULL);
    assert(srcBuf != NULL);
    assert(srcLen > 0);

    srcBuf += pSegHdr->GetDispData();
    srcLen -= pSegHdr->GetDispData();
    if (srcLen < 0) {
        BufPrintf("GLITCH: ran out of data\r\n");
        return;
    }

    if (*srcBuf == 0xf2 && srcLen >= 5) {
        /* handle kSegOpLCONST just for fun */
        subLen = Get32LE(srcBuf+1);
        BufPrintf("OMF: LCONST record (0x%04x bytes follow)\r\n", subLen);
        offset = 5;
    }
    OutputSection(srcBuf + offset,
        pSegHdr->GetSegmentLen() - pSegHdr->GetDispData() - offset,
        0x000000, shortRegs);

#if 0
    OMFSegment seg;
    seg.Setup(pSegHdr, srcBuf, srcLen);
    const uint8_t* ptr;

    do {
        ptr = seg.ProcessNextChunk();
        if (ptr == NULL) {
            BufPrintf("!!! bogus OMF values encountered\r\n");
            return;
        }

        switch (*ptr) {
        case OMFSegment::kSegOpEND:
            BufPrintf("OMF: END (bytesLeft=%d)\r\n", (ptr+1) - srcBuf);
            break;
        }
    } while (*ptr != OMFSegment::kSegOpEND);
#endif
}


/*
 * ===========================================================================
 *      OMFSegmentHeader
 * ===========================================================================
 */

/*
 * Unpack an OMF segment header from the current offset.
 *
 * Returns "true" on success, "false" on failure.
 */
bool OMFSegmentHeader::Unpack(const uint8_t* srcBuf, long srcLen, int fileType)
{
    if (srcLen < kHdrMinSize) {
        LOGI("OMF: Too short to be segment (%ld)", srcLen);
        return false;
    }

    fVersion = *(srcBuf + 0x0f);
    if (fVersion > kMaxVersion) {
        LOGI("OMF: Wrong version number to be OMF (%d)", fVersion);
        return false;
    }

    if (fVersion == 0) {
        /* unpack OMF v1.0 */
        if (srcLen < kV0HdrMinSize)
            return false;

        fBlockCnt = Get32LE(srcBuf + 0x00);
        fResSpc = Get32LE(srcBuf + 0x04);
        fLength = Get32LE(srcBuf + 0x08);
        fType = *(srcBuf + 0x0c);
        fLabLen = *(srcBuf + 0x0d);
        fNumLen = *(srcBuf + 0x0e);
        fBankSize = Get32LE(srcBuf + 0x10);
        fOrg = Get32LE(srcBuf + 0x14);
        fAlign = Get32LE(srcBuf + 0x18);
        fNumSex = *(srcBuf + 0x1c);

        fByteCnt = fBlockCnt * 512;
        fKind = 0;
        fLCBank = 0;
        fSegNum = 0;
        fEntry = 0;
        fTempOrg = 0;
        fDispName = 0x24;
        if (fLabLen == 0)
            fDispData = fDispName + *(srcBuf + fDispName);
        else
            fDispData = fDispName + fLabLen;
    } else if (fVersion == 1) {
        /* unpack OMF v2.0 */
        if (srcLen < kV1HdrMinSize)
            return false;
        fBlockCnt = Get32LE(srcBuf + 0x00);
        fResSpc = Get32LE(srcBuf + 0x04);
        fLength = Get32LE(srcBuf + 0x08);
        fType = *(srcBuf + 0x0c);
        fLabLen = *(srcBuf + 0x0d);
        fNumLen = *(srcBuf + 0x0e);
        fBankSize = Get32LE(srcBuf + 0x10);
        // unused32 at 0x14
        fOrg = Get32LE(srcBuf + 0x18);
        fAlign = Get32LE(srcBuf + 0x1c);
        fNumSex = *(srcBuf + 0x20);
        fLCBank = *(srcBuf + 0x21);
        fSegNum = Get16LE(srcBuf + 0x22);
        fEntry = Get32LE(srcBuf + 0x24);
        fDispName = Get16LE(srcBuf + 0x28);
        fDispData = Get16LE(srcBuf + 0x2a);

        /*
         * Orca/APW libs seem to use version 1 with a byte count in the
         * first field, but the "LLRE" app has version 1 with a block count.
         * The spec is pretty clear, so it looks like somebody's library
         * builder screwed up.
         *
         * Special case type=LIB ($B2).
         */
        if (fileType == 0xb2) {
            LOGI("NOTE: switching blockCount=%ld to byte count", fBlockCnt);
            fByteCnt = fBlockCnt;
            fBlockCnt = 0;
        } else {
            fByteCnt = fBlockCnt * 512;
        }
        fKind = 0;
        fTempOrg = 0;
    } else {
        /* unpack OMF v2.1 */
        if (srcLen < kV2HdrMinSize)
            return false;
        fByteCnt = Get32LE(srcBuf + 0x00);
        fResSpc = Get32LE(srcBuf + 0x04);
        fLength = Get32LE(srcBuf + 0x08);
        // unused at +0x0c
        fLabLen = *(srcBuf + 0x0d);
        fNumLen = *(srcBuf + 0x0e);
        fBankSize = Get32LE(srcBuf + 0x10);
        fKind = Get16LE(srcBuf + 0x14);
        // unused at +0x16
        fOrg = Get32LE(srcBuf + 0x18);
        fAlign = Get32LE(srcBuf + 0x1c);
        fNumSex = *(srcBuf + 0x20);
        fSegNum = Get16LE(srcBuf + 0x22);
        // unused at +0x23
        fEntry = Get32LE(srcBuf + 0x24);
        fDispName = Get16LE(srcBuf + 0x28);
        fDispData = Get16LE(srcBuf + 0x2a);
        fTempOrg = Get32LE(srcBuf + 0x2c);

        fBlockCnt = 0;
        fType = 0;
        fLCBank = 0;
    }


    /* validate fields */
    if (fByteCnt < kHdrMinSize || fByteCnt > (uint32_t) srcLen) {
        LOGI("OMF: Bad value for byteCnt (%ld, srcLen=%ld min=%d)",
            fByteCnt, srcLen, kHdrMinSize);
        return false;
    }
    if (fDispName < 0x24 || fDispName > (srcLen - kLoadNameLen)) {
        LOGI("OMF: Bad value for dispName (%d, srcLen=%ld)",
            fDispName, srcLen);
        return false;
    }
    if (fDispData < 0x24 || fDispData > srcLen) {
        LOGI("OMF: Bad value for dispData (%d, srcLen=%ld)",
            fDispData, srcLen);
        return false;
    }
    if (fDispData < fDispName + kLoadNameLen) {
        LOGI("OMF: dispData is inside label region (%d / %d)",
            fDispData, fDispName);
        return false;
    }
    if (fBankSize != kExpectedBankSize && fBankSize != 0) {
        LOGI("OMF: NOTE: bankSize=%ld", fBankSize);
        /* allowed, just weird */
    }
    if (fNumLen != kExpectedNumLen || fNumSex != 0) {
        LOGI("OMF: WARNING: numLen=%d numSex=%d", fNumLen, fNumSex);
        /* big endian odd-sized numbers?? keep going, I guess */
    }

    const uint8_t* segName;
    int segLabelLen;

    /* copy the label entries over */
    segName = srcBuf + fDispName;
    if (fVersion > 0) {
        memcpy(fLoadName, srcBuf + fDispName, kLoadNameLen);
        fLoadName[kLoadNameLen] = '\0';

        segName +=  kLoadNameLen;
    }

    if (fLabLen == 0) {
        /* pascal-style string */
        segLabelLen = *segName++;
        memcpy(fSegName, segName, segLabelLen);
        fSegName[segLabelLen] = '\0';
        LOGI(" OMF: Pascal segment label '%hs'", fSegName);
    } else {
        /* C-style or non-terminated string */
        segLabelLen = fLabLen;
        memcpy(fSegName, segName, segLabelLen);
        fSegName[segLabelLen] = '\0';
        LOGI(" OMF: Std segment label '%hs'", fSegName);
    }

    fReady = true;
    return true;
}


/*
 * Pry the segment type out of the "type" or "kind" field.
 */
OMFSegmentHeader::SegmentType OMFSegmentHeader::GetSegmentType(void) const
{
    assert(fReady);

    if (fVersion < 2)
        return (SegmentType) (fType & 0x1f);
    else
        return (SegmentType) (fKind & 0x1f);
}

/*
 * Return the value of one of the segment header flags.
 */
bool OMFSegmentHeader::GetSegmentFlag(SegmentFlag flag) const
{
    if (fVersion < 2) {
        switch (flag) {
        case kFlagPositionIndep:
            return (fType & 0x20) != 0;
        case kFlagPrivate:
            return (fType & 0x40) != 0;
        case kFlagDynamic:
            return (fType & 0x80) != 0;
        default:
            return false;
        }
    } else {
        switch (flag) {
        case kFlagBankRelative:
            return (fKind & 0x0100) != 0;
        case kFlagSkip:
            return (fKind & 0x0200) != 0;
        case kFlagReload:
            return (fKind & 0x0400) != 0;
        case kFlagAbsoluteBank:
            return (fKind & 0x0800) != 0;
        case kFlagNoSpecialMem:
            return (fKind & 0x1000) != 0;
        case kFlagPositionIndep:
            return (fKind & 0x2000) != 0;
        case kFlagPrivate:
            return (fKind & 0x4000) != 0;
        case kFlagDynamic:
            return (fKind & 0x8000) != 0;
        default:
            assert(false);
            return false;
        }
    }
}

/*
 * Dump the contents of the segment header struct.
 */
void OMFSegmentHeader::Dump(void) const
{
    LOGI("OMF segment header:");
    LOGI("  segNum=%d loadName='%hs' segName='%hs'",
        fSegNum, fLoadName, fSegName);
    LOGI("  blockCnt=%ld byteCnt=%ld resSpc=%ld length=%ld",
        fBlockCnt, fByteCnt, fResSpc, fLength);
    LOGI("  version=%d type=0x%02x kind=0x%04x",
        fVersion, fType, fKind);
    LOGI("  labLen=%d numLen=%d bankSize=%ld lcBank=%d",
        fLabLen, fNumLen, fBankSize, fLCBank);
    LOGI("  align=%ld numSex=%d org=%ld tempOrg=%ld",
        fAlign, fNumSex, fOrg, fTempOrg);
    LOGI("  entry=%ld dispName=%d dispData=%d",
        fEntry, fDispName, fDispData);
}

#if 0
/*
 * ===========================================================================
 *      OMFSegment
 * ===========================================================================
 */

/*
 * Prepare to roll through a segment.
 */
void
OMFSegment::Setup(const OMFSegmentHeader* pSegHdr, const uint8_t* srcBuf,
    long srcLen)
{
    fSegBuf = fCurPtr = srcBuf + pSegHdr->GetDispData();
    fSegLen = srcLen - pSegHdr->GetDispData();

    fLabLen = pSegHdr->GetLabLen();
    fNumLen = pSegHdr->GetNumLen();
}

/*
 * Process the next chunk from the segment.
 *
 * Returns a pointer to the start of the chunk, or "NULL" if we've encountered
 * some bogus condition (e.g. running off the end).
 */
const uint8_t*
OMFSegment::ProcessNextChunk(void)
{
    const uint8_t* prevPtr = fCurPtr;
    long remLen = fSegLen - (fCurPtr - fSegBuf);
    uint32_t subLen;
    int len = 1;        // one byte at least (for the opcode)

    assert(fLabLen >= 0);
    assert(fNumLen > 0);

    switch (*fCurPtr) {
    case kSegOpEND:
        LOGI("  OMF END reached, remaining len = %d",
            fSegLen - (fCurPtr - fSegBuf));
        assert(len == 1);
        break;
    case kSegOpALIGN:
        len += fNumLen;
        break;
    case kSegOpORG:
        len += fNumLen;
        break;
    case kSegOpRELOC:
        len += 10;
        break;
    case kSegOpINTERSEG:
        len += 14;
        break;
    case kSegOpUSING:
    case kSegOpSTRONG:
        len += LabelLength(fCurPtr + 1);
        break;
    case kSegOpGLOBAL:
        len += LabelLength(fCurPtr + 1) + 4;
        break;
    case kSegOpGEQU:
        len += LabelLength(fCurPtr + 1) + 4;
        len += ExpressionLength(fCurPtr + len);
        break;
    case kSegOpMEM:     // not used on IIgs?
        len += fNumLen*2;
        break;
    case kSegOpEXPR:
    case kSegOpZEXPR:
    case kSegOpBEXPR:
        len += 1;
        len += ExpressionLength(fCurPtr + len);
        break;
    case kSegOpRELEXPR:
        len += 1 + fNumLen;
        len += ExpressionLength(fCurPtr + len);
        break;
    case kSegOpLOCAL:
        len += LabelLength(fCurPtr+1) + 4;
        break;
    case kSegOpEQU:
        len += LabelLength(fCurPtr+1) + 4;
        len += ExpressionLength(fCurPtr + len);
        break;
    case kSegOpDS:
        len += fNumLen;
        break;
    case kSegOpLCONST:
        subLen = Get32LE(fCurPtr+1);
        len += fNumLen + subLen;
        break;
    case kSegOpLEXPR:
        len += 1;
        len += ExpressionLength(fCurPtr + len);
        break;
    case kSegOpENTRY:
        len += 6;
        len += LabelLength(fCurPtr + len);
        break;
    case kSegOpcRELOC:
        len += 6;
        break;
    case kSegOpcINTERSEG:
        len += 7;
        break;
    case kSegOpSUPER:
        subLen = Get32LE(fCurPtr+1);
        len += 4 + subLen;
        break;

    case kSegOpGeneral:
    case kSegOpExperimental1:
    case kSegOpExperimental2:
    case kSegOpExperimental3:
    case kSegOpExperimental4:
        subLen = Get32LE(fCurPtr+1);    // assumes fNumLen==4
        LOGI("  OMF found 'reserved' len=%lu (remLen=%ld)", subLen, remLen);
        if (subLen > (uint32_t) remLen)
            return NULL;
        len += subLen + fNumLen;
        break;
    default:
        assert(len == 1);
        if (*fCurPtr >= kSegOpCONSTStart && *fCurPtr <= kSegOpCONSTEnd)
            len += *fCurPtr;
        break;
    }

    fCurPtr += len;

    return prevPtr;
}

/*
 * Determine the length of an OMF expression.
 *
 * Pass a pointer to the start of the expression.
 */
int
OMFSegment::ExpressionLength(const uint8_t* ptr)
{
    // do this someday
    return 1;
}
#endif
