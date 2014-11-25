/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat double-hi-res graphics.
 */
#ifndef REFORMAT_DOUBLEHIRES_H
#define REFORMAT_DOUBLEHIRES_H

#include "ReformatBase.h"

/*
 * Reformat double-hi-res graphics into a bitmap.
 */
class ReformatDHR : public ReformatGraphics {
public:
    ReformatDHR(void) {}
    virtual ~ReformatDHR(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

    enum {
        kPixelsPerLine = 560,
        kNumLines = 192,

        kOutputWidth = 560,
        kOutputHeight = 384,
        kPageSize = 8192,
        kExpectedSize = kPageSize*2,
        kNumDHRColors = 16,
    };

    /* this MUST match up with prefs ctrl indices (IDC_DHR_CONV_COMBO) */
    typedef enum {
        kDHRBlackWhite = 0,
        kDHRLatched = 1,
        kDHRPlain140 = 2,
        kDHRWindow = 3,
    } Algorithms;

    void InitColorLookup(void);
    MyDIBitmap* DHRScreenToBitmap(const uint8_t* buf);

    Algorithms  fAlgorithm;
    int         fLineOffset[kNumLines];
    unsigned int fColorLookup[4][kNumDHRColors];
};

#endif /*REFORMAT_DOUBLEHIRES_H*/
