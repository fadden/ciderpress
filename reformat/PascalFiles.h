/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat UCSD Pascal files.
 */
#ifndef REFORMAT_PASCALFILES_H
#define REFORMAT_PASCALFILES_H

#include "ReformatBase.h"

/*
 * Reformat a Pascal code file into its constituent parts.
 */
class ReformatPascalCode : public ReformatText {
public:
    ReformatPascalCode(void) {}
    virtual ~ReformatPascalCode(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

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
        uint8_t         segNum;
        MachineType     mType;
        uint8_t         unused;
        uint8_t         version;
    } SegInfo;
    typedef struct PCDSegment {
        uint16_t        codeLeng;
        uint16_t        codeAddr;
        char            name[kSegmentNameLen+1];
        SegmentKind     segmentKind;
        uint16_t        textAddr;
        SegInfo         segInfo;
    } PCDSegment;

    void PrintSegment(PCDSegment* pSegment, int segNum,
        const uint8_t* srcBuf, long srcLen);
};

/*
 * Reformat a Pascal text file into plain text.
 */
class ReformatPascalText : public ReformatText {
public:
    ReformatPascalText(void) {}
    virtual ~ReformatPascalText(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
    enum {
        kPTXBlockSize = 1024,
        kDLE = 0x10,
        kIndentSub = 32,
    };

private:
    void ProcessBlock(const uint8_t* srcBuf, long length);
};

#endif /*REFORMAT_PASCALFILES_H*/
