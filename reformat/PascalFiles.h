/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat UCSD Pascal files.
 */
#ifndef __LR_PASCALFILES__
#define __LR_PASCALFILES__

#include "ReformatBase.h"

/*
 * Reformat a Pascal code file into its constituent parts.
 */
class ReformatPascalCode : public ReformatText {
public:
    ReformatPascalCode(void) {}
    virtual ~ReformatPascalCode(void) {}

    virtual void Examine(ReformatHolder* pHolder);
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput);

    enum {
        // VC++6.0 demands this be public, probably because it's used to
        // determine the size of the object.
        kSegmentNameLen     = 8,
    };

private:
    enum {
        kSegmentHeaderLen   = 512,
        kNumSegments        = 16,
    };
    typedef enum SegmentKind {
        kSegmentLinked,
        kSegmentHostseg,
        kSegmentSegproc,
        kSegmentUnitseg,
        kSegmentSeprtseg,
        kSegmentUnlinkedIntrins,
        kSegmentLinkedIntrins,
        kSegmentDataseg
    } SegmentKind;
    typedef enum MachineType {
        kMTUnidentified = 0,
        kMTPCodeMSB         = 1,
        kMTPCodeLSB         = 2,
        kMTPAsm3            = 3,
        kMTPAsm4            = 4,
        kMTPAsm5            = 5,
        kMTPAsm6            = 6,
        kMTPAsmApple6502    = 7,
        kMTPAsm8            = 8,
        kMTPAsm9            = 9,
    } MachineType;
    typedef struct SegInfo {
        unsigned char segNum;
        MachineType   mType;
        unsigned char unused;
        unsigned char version;
    } SegInfo;
    typedef struct PCDSegment {
        unsigned short  codeLeng;
        unsigned short  codeAddr;
        char            name[kSegmentNameLen+1];
        SegmentKind     segmentKind;
        unsigned short  textAddr;
        SegInfo         segInfo;
    } PCDSegment;

    void PrintSegment(PCDSegment* pSegment, int segNum,
        const unsigned char* srcBuf, long srcLen);
};

/*
 * Reformat a Pascal text file into plain text.
 */
class ReformatPascalText : public ReformatText {
public:
    ReformatPascalText(void) {}
    virtual ~ReformatPascalText(void) {}

    virtual void Examine(ReformatHolder* pHolder);
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput);

private:
    enum {
        kPTXBlockSize = 1024,
        kDLE = 0x10,
        kIndentSub = 32,
    };

private:
    void ProcessBlock(const unsigned char* srcBuf, long length);
};

#endif /*__LR_PASCALFILES__*/
