/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat hi-res images.
 */
#ifndef REFORMAT_HIRES_H
#define REFORMAT_HIRES_H

#include "ReformatBase.h"

/*
 * Reformat a HiRes graphic into a bitmap.
 */
class ReformatHiRes : public ReformatGraphics {
public:
    ReformatHiRes(void) { fBlackWhite = false; }
    virtual ~ReformatHiRes(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

    static void InitLineOffset(int* pOffsetBuf);

private:
    enum {
        kPixelsPerLine = 280,
        kNumLines = 192,

        kOutputWidth = 560,
        kOutputHeight = 384,
        kExpectedSize = 8192,
    };

    MyDIBitmap* HiResScreenToBitmap(const uint8_t* buf);

    static long ExpandLZ4FH(uint8_t* dstBuf, const uint8_t* srcBuf,
        long srcLen);

    int     fLineOffset[kNumLines];
    bool    fBlackWhite;
};

#endif /*REFORMAT_HIRES_H*/
