/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Process UCSD Pascal text and code files.  Files with this format, but
 * usually containing 6502 assembly rather than p-code, can appear on
 * Apple /// SOS disks.
 */
#include "StdAfx.h"
#include "PascalFiles.h"

/*
 * ===========================================================================
 *      Pascal Code
 * ===========================================================================
 */

/*
 * Decide whether or not we want to handle this file.
 */
void ReformatPascalCode::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;

    if (pHolder->GetFileType() == kTypePCD)
        applies = ReformatHolder::kApplicProbably;

    pHolder->SetApplic(ReformatHolder::kReformatPascalCode, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Break a Pascal code file down into its separate components.
 *
 * The file format is described on page 266 of the "Apple Pascal Operating
 * System Reference Manual".  The first 512-byte block has a header; following
 * that are a series of up to 16 segments with Stuff in them.
 *
 * Rather than dump the header and follow it with bits and pieces, we gather
 * up all the header data and present it with the contents of the block.
 */
int ReformatPascalCode::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    fUseRTF = false;
    int retval = -1;
    PCDSegment segments[kNumSegments];
    uint32_t intrinsSegs;
    int i;

    if (srcLen < kSegmentHeaderLen) {
        LOGI("  PCD truncated?");
        goto bail;
    }

    RTFBegin();

    /*
     * Pull the data fields out of srcBuf.
     */
    for (i = 0; i < kNumSegments; i++) {
        uint16_t segInfo;

        segments[i].codeAddr = Get16LE(srcBuf + 0x00 + i*4);
        segments[i].codeLeng = Get16LE(srcBuf + 0x02 + i*4);
        memcpy(segments[i].name, srcBuf + 0x40 + i*kSegmentNameLen, kSegmentNameLen);
        segments[i].name[kSegmentNameLen] = '\0';
        segments[i].segmentKind = (SegmentKind) Get16LE(srcBuf + 0xc0 + i*2);
        segments[i].textAddr = Get16LE(srcBuf + 0xe0 + i*2);
        segInfo = Get16LE(srcBuf + 0x100 + i*2);
        segments[i].segInfo.segNum = segInfo & 0xff;
        segments[i].segInfo.mType = (MachineType) ((segInfo >> 8) & 0x0f);
        segments[i].segInfo.unused = (segInfo >> 12) & 0x01;
        segments[i].segInfo.version = (segInfo >> 13) & 0x07;
    }
    intrinsSegs = Get32LE(srcBuf + 0x120);

    int numSegments;
    numSegments = 0;
    for (i = 0; i < kNumSegments; i++) {
        if (segments[i].codeAddr != 0 ||
            segments[i].codeLeng != 0)
                numSegments++;
    }

    /*
     * Print the header.
     */
    BufPrintf("Pascal code file has %d segment%s\r\n", numSegments,
        numSegments == 1 ? "" : "s");
    BufPrintf("Intrinsic units required:");
    if (intrinsSegs == 0)
        BufPrintf(" none");
    else {
        for (i = 0; i < 32; i++) {
            if ((intrinsSegs & 0x01) != 0)
                BufPrintf(" %d", i);
            intrinsSegs >>= 1;
        }
    }
    BufPrintf("\r\n");

#if 0       // region is undefined; see the Pilot disk for weird examples
    /*
     * Look for a string in the header.
     */
    for (i = 0x124; i < 512; i++) {
        int strLen = srcBuf[i];
        if (strLen != 0 && (512 - (i+strLen+1)) > 0) {
            char* tmpBuf = new char[strLen+1];
            memcpy(tmpBuf, srcBuf + i +1, strLen);
            tmpBuf[strLen] = '\0';
            BufPrintf("Header string found: '%s'\r\n", tmpBuf);
            delete[] tmpBuf;

            i += strLen;
        }
    }
#endif

    //BufPrintf("Leftover stuff in segment dictionary block:\r\n");
    //BufHexDump(srcBuf + 0x124, 512 - 0x124);

    for (i = 0; i < kNumSegments; i++)
        PrintSegment(&segments[i], i, srcBuf, srcLen);

    RTFEnd();

    SetResultBuffer(pOutput);
    retval = 0;

bail:
    return retval;
}

/*
 * Print information about and the contents of one segment.
 */
void ReformatPascalCode::PrintSegment(PCDSegment* pSegment, int segNum,
    const uint8_t* srcBuf, long srcLen)
{
    const char* segKindStr;
    const char* mTypeStr;

    if (pSegment->codeAddr == 0 && pSegment->codeLeng == 0)
        return;

    switch (pSegment->segmentKind) {
    case kSegmentLinked:            segKindStr = "LINKED";      break;
    case kSegmentHostseg:           segKindStr = "HOSTSEG";     break;
    case kSegmentSegproc:           segKindStr = "SEGPROC";     break;
    case kSegmentUnitseg:           segKindStr = "UNITSEG";     break;
    case kSegmentSeprtseg:          segKindStr = "SEPRTSEG";    break;
    case kSegmentUnlinkedIntrins:   segKindStr = "UNLINKED_INTRINS";    break;
    case kSegmentLinkedIntrins:     segKindStr = "LINKED_INTRINS";      break;
    case kSegmentDataseg:           segKindStr = "DATASEG";     break;
    default:                        segKindStr = "UNKNOWN";     break;
    };
    switch (pSegment->segInfo.mType) {
    case kMTUnidentified:       mTypeStr = "unidentified";          break;
    case kMTPCodeMSB:           mTypeStr = "P-Code (MSB first)";    break;
    case kMTPCodeLSB:           mTypeStr = "P-Code (LSB first)";    break;
    case kMTPAsm3:              mTypeStr = "Machine code (type 3)"; break;
    case kMTPAsm4:              mTypeStr = "Machine code (type 4)"; break;
    case kMTPAsm5:              mTypeStr = "Machine code (type 5)"; break;
    case kMTPAsm6:              mTypeStr = "Machine code (type 6)"; break;
    case kMTPAsmApple6502:      mTypeStr = "Apple II 6502 machine code";    break;
    case kMTPAsm8:              mTypeStr = "Machine code (type 8)"; break;
    case kMTPAsm9:              mTypeStr = "Machine code (type 9)"; break;
    default:                    mTypeStr = "unknown";               break;
    };

    BufPrintf("\r\n");
    BufPrintf("Segment %d: '%s' (%s)\r\n", segNum, pSegment->name, segKindStr);
    BufPrintf("  Segment start block: %d\r\n", pSegment->codeAddr);
    BufPrintf("  Segment length: %d\r\n", pSegment->codeLeng);
    BufPrintf("  Text address: %d\r\n", pSegment->textAddr);
    BufPrintf("  Segment info: segNum=%d version=%d mType=%s\n",
        pSegment->segInfo.segNum, pSegment->segInfo.version, mTypeStr);
    BufPrintf("\r\n");

    if (pSegment->codeAddr == 0) {
        if (pSegment->segmentKind == kSegmentDataseg) {
            BufPrintf("(no data for DATASEG segments)\r\n");
        } else {
            BufPrintf("Segment start block of zero not expected.\r\n");
        }
    } else {
        if (pSegment->codeAddr * 512 + pSegment->codeLeng > srcLen) {
            BufPrintf("INVALID DATA POINTER\r\n");
        } else {
            BufHexDump(srcBuf + pSegment->codeAddr * 512, pSegment->codeLeng);
        }
    }
}


/*
 * ===========================================================================
 *      Pascal Text
 * ===========================================================================
 */

/*
 * Decide whether or not we want to handle this file.
 */
void ReformatPascalText::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;

    if (pHolder->GetFileType() == kTypePTX)
        applies = ReformatHolder::kApplicProbably;

    pHolder->SetApplic(ReformatHolder::kReformatPascalText, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert Pascal text to plain text.
 *
 * The file format is documented on page 266 of the classic "Apple Pascal
 * Operating System Reference Manual".  Basically it's set in 1024-byte
 * blocks, where each line can start with an optional DLE (0x10) indicating
 * that the next value is 32+indent.  Lines end with a CR.  If the next line
 * won't fit in the block, the remainder of the block is filled with zeroes.
 */
int ReformatPascalText::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    long length = srcLen;
    fUseRTF = false;
    int retval = -1;

    if (srcLen < kPTXBlockSize) {
        LOGI("  PTX truncated?");
        goto bail;
    }

    RTFBegin();

    /* the first block is filled with editor storage */
    srcBuf += kPTXBlockSize;
    length -= kPTXBlockSize;

    while (length) {
        int blockLen = length > kPTXBlockSize ? kPTXBlockSize : length;

        ProcessBlock(srcBuf, blockLen);

        srcBuf += blockLen;
        length -= blockLen;
    }

    RTFEnd();

    SetResultBuffer(pOutput);
    retval = 0;

bail:
    return retval;
}

/*
 * Process up to 1024 bytes of text.
 *
 * I'm not sure if the format mandates full 1024-byte blocks -- it
 * appears to -- but I'm not going to assume it.
 */
void ReformatPascalText::ProcessBlock(const uint8_t* srcBuf, long length)
{
    ASSERT(srcBuf != NULL);
    ASSERT(length > 0 && length <= kPTXBlockSize);
    
    char lineBuf[kPTXBlockSize+1];
    char* linePtr;
    int indent;

    while (length) {
        if (*srcBuf == 0x00) {
            /* we've reached the end of the data for this block */
            LOGI(" PTX end of useful block with %d remaining", length);

            /* be paranoid */
            bool first = true;
            while (length--) {
                if (*srcBuf != 0x00) {
                    if (first) {
                        BufPrintf("EXTRA: ");
                        first = false;
                    }
                    BufPrintf("%c", *srcBuf);
                }
                srcBuf++;
            }
            if (!first) {
                RTFNewPara();
            }
            goto bail;
        }
        if (*srcBuf == kDLE) {
            srcBuf++;
            length--;
            if (!length) {
                LOGI(" PTX end of block inside DLE");
                goto bail;
            }
            indent = *srcBuf - kIndentSub;
            if (indent < 0) {
                LOGI(" PTX odd indent (raw value %d)", *srcBuf);
                indent = 0;     /* fix it */
            }
            srcBuf++;
            length--;

            /* print the #of spaces indicated */
            linePtr = lineBuf;
            while (indent--)
                *linePtr++ = ' ';
            *linePtr = '\0';
            BufPrintf("%s", lineBuf);

            if (!length)
                goto bail;
        }

        ASSERT(length > 0);

        /*
         * Accumulate the line into a buffer and then spit it out all
         * at once.
         */
        linePtr = lineBuf;
        while (*srcBuf != 0x0d && length) {
            if (*srcBuf == 0x00) {
                LOGI(" PTX a null leaked into a line??");
                /* keep going */
            }

            *linePtr++ = *srcBuf++;
            length--;
        }
        if (length && *srcBuf == 0x0d) {
            srcBuf++;
            length--;
        }
        *linePtr = '\0';

        BufPrintf("%s", lineBuf);
        RTFNewPara();
    }

bail:
    return;
}
